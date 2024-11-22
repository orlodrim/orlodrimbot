#include "sqlite.h"
#include <sqlite3.h>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <utility>
#include "error.h"
#include "file.h"
#include "log.h"

using std::string;

namespace sqlite {

constexpr int LONG_TRANSACTION_WARNING_THRESHOLD_SECS = 240;
constexpr int BUSY_TIMEOUT_SECS = 300;

[[noreturn]] static void throwSqliteError(int code, const string& message) {
  char codeStr[14];
  snprintf(codeStr, sizeof(codeStr), " (0x%X)", code);
  string messageWithCode = message + codeStr;
  if (code == SQLITE_CONSTRAINT_PRIMARYKEY) {
    throw PrimaryKeyConstraintError(messageWithCode);
  } else if (code == SQLITE_CONSTRAINT_UNIQUE) {
    throw UniqueConstraintError(messageWithCode);
  }
  int lowerByte = code & 0xFF;
  if (lowerByte == SQLITE_BUSY) {
    throw BusyError(messageWithCode);
  } else if (lowerByte == SQLITE_READONLY) {
    throw ReadOnlyError(messageWithCode);
  }
  throw SqliteError(messageWithCode);
}

Statement::Statement() {}

Statement::Statement(Database* database, const string& text) : m_stmt(nullptr), m_database(database), m_text(text) {
  CBL_ASSERT(database && database->m_db);
  m_requiresWriteLock = strncmp(text.c_str(), "SELECT ", strlen("SELECT ")) != 0;
  const int prepareResult = sqlite3_prepare_v2(database->m_db, text.c_str(), -1, &m_stmt, nullptr);
  if (prepareResult != SQLITE_OK) {
    throw SqliteError("Preparation of statement '" + text + "' failed: " + sqlite3_errmsg(database->m_db));
  }
  m_numParameters = sqlite3_bind_parameter_count(m_stmt);
}

Statement::Statement(Statement&& statement)
    : m_stmt(statement.m_stmt), m_database(statement.m_database), m_text(std::move(statement.m_text)),
      m_numParameters(statement.m_numParameters), m_requiresWriteLock(statement.m_requiresWriteLock) {
  statement.m_stmt = nullptr;
}

Statement& Statement::operator=(Statement&& statement) {
  if (&statement != this) {
    sqlite3_finalize(m_stmt);
    m_stmt = statement.m_stmt;
    m_database = statement.m_database;
    m_text = std::move(statement.m_text);
    m_numParameters = statement.m_numParameters;
    m_requiresWriteLock = statement.m_requiresWriteLock;
    statement.m_stmt = nullptr;
  }
  return *this;
}

Statement::~Statement() {
  sqlite3_finalize(m_stmt);
}

void Statement::reset() {
  CBL_ASSERT(m_stmt);
  sqlite3_reset(m_stmt);
  sqlite3_clear_bindings(m_stmt);
}

bool Statement::step() {
  CBL_ASSERT(m_stmt);
  if (!m_database->m_transaction) {
    throw NotInTransactionError("Attempt to execute statement '" + m_text + "' outside of a transaction");
  } else if (m_requiresWriteLock && !m_database->m_transaction->exclusive()) {
    throw NotInTransactionError("Attempt to execute statement '" + m_text + "' outside of a write transaction");
  }
  int stepResult = sqlite3_step(m_stmt);
  if (stepResult == SQLITE_ROW) {
    return true;
  } else if (stepResult != SQLITE_DONE) {
    throwSqliteError(stepResult, "Error while executing statement '" + m_text + "': " + m_database->lastErrorMessage());
  }
  return false;
}

void Statement::bind(int parameter, int value) {
  CBL_ASSERT(m_stmt);
  int bindResult = sqlite3_bind_int(m_stmt, parameter, value);
  if (bindResult != SQLITE_OK) {
    throwSqliteError(bindResult, "sqlite3_bind_int failed: " + m_database->lastErrorMessage());
  }
}

void Statement::bind(int parameter, int64_t value) {
  CBL_ASSERT(m_stmt);
  int bindResult = sqlite3_bind_int64(m_stmt, parameter, value);
  if (bindResult != SQLITE_OK) {
    throwSqliteError(bindResult, "sqlite3_bind_int64 failed: " + m_database->lastErrorMessage());
  }
}

void Statement::bind(int parameter, double value) {
  CBL_ASSERT(m_stmt);
  int bindResult = sqlite3_bind_double(m_stmt, parameter, value);
  if (bindResult != SQLITE_OK) {
    throwSqliteError(bindResult, "sqlite3_bind_double failed: " + m_database->lastErrorMessage());
  }
}

void Statement::bind(int parameter, const char* value) {
  CBL_ASSERT(m_stmt);
  int bindResult = sqlite3_bind_text(m_stmt, parameter, value, -1, SQLITE_TRANSIENT);
  if (bindResult != SQLITE_OK) {
    throwSqliteError(bindResult, "sqlite3_bind_text failed: " + m_database->lastErrorMessage());
  }
}

void Statement::bindBlob(int parameter, const char* value, int length) {
  CBL_ASSERT(m_stmt);
  int bindResult = sqlite3_bind_blob(m_stmt, parameter, value, length, SQLITE_TRANSIENT);
  if (bindResult != SQLITE_OK) {
    throwSqliteError(bindResult, "sqlite3_bind_blob failed: " + m_database->lastErrorMessage());
  }
}

void Statement::bindNull(int parameter) {
  CBL_ASSERT(m_stmt);
  int bindResult = sqlite3_bind_null(m_stmt, parameter);
  if (bindResult != SQLITE_OK) {
    throwSqliteError(bindResult, "sqlite3_bind_null failed: " + m_database->lastErrorMessage());
  }
}

void Statement::bindFromIndex(int index) {
  // The expected value is index = m_numParameters + 1.
  // If there are too many arguments, this should be detected earlier (sqlite3_bind_* would fail).
  if (index <= m_numParameters) {
    // SqliteError is consistent with what happens when there are too many arguments.
    throw SqliteError("Not enough parameters to initialize statement");
  }
}

Transaction::Transaction(Database& database, bool exclusive, const char* name)
    : m_database(database), m_exclusive(exclusive), m_name(name) {
  database.beginTransaction(this);
}

Transaction::~Transaction() {
  if (m_database.m_transaction == this) {
    m_database.endTransaction(/* commit = */ false);
  }
}

void WriteTransaction::commit() {
  if (m_database.m_transaction != this) {
    throw NotInTransactionError(string("Attempt to commit transaction '") + m_name + "' after it has ended");
  }
  m_database.endTransaction(/* commit = */ true);
}

Database Database::open(const string& path, const OpenParams& params, const InitCallback& initCallback) {
  Database database;
  database.openInternal(path, params, initCallback);
  return database;
}

bool Database::synchronousModeForcedOff = false;

Database::Database() {}

Database::Database(Database&& database) : m_db(database.m_db) {
  if (database.m_transaction) {
    throw cbl::InvalidStateError("Locked database cannot be moved");
  }
  database.m_db = nullptr;
}

Database::~Database() {
  try {
    close();
  } catch (const SqliteError& e) {
    CBL_ERROR << e.what();
  }
}

Database& Database::operator=(Database&& database) {
  if (database.m_transaction) {
    throw cbl::InvalidStateError("Locked database cannot be moved");
  }
  if (&database != this) {
    close();
    m_db = database.m_db;
    database.m_db = nullptr;
  }
  return *this;
}

void Database::openInternal(const string& path, const OpenParams& params, const InitCallback& initCallback) {
  CBL_ASSERT(m_db == nullptr);
  int flags = 0;
  switch (params.openMode) {
    case OPEN_READONLY:
      flags = SQLITE_OPEN_READONLY;
      break;
    case OPEN_READWRITE:
      flags = SQLITE_OPEN_READWRITE;
      break;
    case OPEN_OR_CREATE:
      flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
      break;
  }
  int openResult = sqlite3_open_v2(path.c_str(), &m_db, flags, nullptr);
  if (openResult != SQLITE_OK) {
    string errorPrefix = "Cannot open '" + path + "': ";
    if ((openResult & 0xFF) == SQLITE_CANTOPEN && params.openMode != OPEN_OR_CREATE && !cbl::fileExists(path)) {
      throw cbl::FileNotFoundError(errorPrefix + "file not found");
    }
    string errorMessage = m_db ? lastErrorMessage() : "Failed to allocate memory for a new database";
    throwSqliteError(openResult, errorPrefix + errorMessage);
  }
  CBL_ASSERT(m_db != nullptr);
  sqlite3_busy_timeout(m_db, BUSY_TIMEOUT_SECS * 1000);
  sqlite3_extended_result_codes(m_db, 1);
  setSynchronousMode(synchronousModeForcedOff ? SYNC_OFF : params.synchronousMode);
  if (params.openMode == OPEN_OR_CREATE) {
    WriteTransaction transaction(*this, "Database::openInternal");
    Statement statement = prepare("SELECT name FROM sqlite_master WHERE type='table' LIMIT 1;");
    if (!statement.step()) {
      execMany(
          "CREATE TABLE sqlitewrapper_table(key TEXT, value TEXT); "
          "CREATE UNIQUE INDEX sqlitewrapper_table_index ON sqlitewrapper_table(key);");
      if (initCallback) {
        initCallback(*this);
      }
    }
    transaction.commit();
  }
}

void Database::close() {
  if (m_db) {
    int closeResult = sqlite3_close(m_db);
    if (closeResult != SQLITE_OK) {
      throwSqliteError(closeResult, closeResult == SQLITE_BUSY
                                        ? "Failed to close database due to unfinalized statements or objects"
                                        : "Failed close database");
    }
    m_db = nullptr;
  }
}

void Database::execMany(const string& statement, LockType requiredLockType) {
  if (requiredLockType != UNLOCKED) {
    if (!m_transaction) {
      throw NotInTransactionError("Attempt to execute statement '" + statement + "' outside of a transaction");
    } else if (requiredLockType == WRITE_LOCK && !m_transaction->exclusive()) {
      throw NotInTransactionError("Attempt to execute statement '" + statement + "' outside of a write transaction");
    }
  }
  string errorMessage;
  int execCode = execManyInternal(statement, errorMessage);
  if (execCode != SQLITE_OK) {
    throwSqliteError(execCode, "Failed to execute statement '" + statement + "': " + errorMessage);
  }
}

int Database::execManyInternal(const string& statement, string& errorMessage) {
  CBL_ASSERT(m_db != nullptr);
  char* errorMessagePtr = nullptr;
  int execResult = sqlite3_exec(m_db, statement.c_str(), nullptr, 0, &errorMessagePtr);
  if (errorMessagePtr) {
    errorMessage = errorMessagePtr;
    sqlite3_free(errorMessagePtr);
  } else {
    errorMessage.clear();
  }
  return execResult;
}

string Database::lastErrorMessage() const {
  const char* message;
  if (m_db) {
    message = sqlite3_errmsg(m_db);
    if (!message) {
      message = "<no error message>";
    }
  } else {
    message = "<failed to retrieve error message because m_db is null>";
  }
  return message;
}

void Database::beginTransaction(Transaction* transaction) {
  if (m_transaction != nullptr) {
    throw NestedTransactionsError(string("Attempt to start transaction '") + transaction->name() +
                                  "' inside transaction '" + m_transaction->name() + "'");
  }
  string statement = transaction->exclusive() ? "BEGIN EXCLUSIVE TRANSACTION;" : "BEGIN TRANSACTION;";
  string errorMessage;
  int execResult = execManyInternal(statement, errorMessage);
  if (execResult != SQLITE_OK) {
    throwSqliteError(execResult, string("Failed to start transaction '") + transaction->name() + "': " + errorMessage);
  }
  m_transaction = transaction;
  m_transactionStartTime = static_cast<int64_t>(time(nullptr));
}

void Database::endTransaction(bool commit) {
  if (m_transaction != nullptr) {
    int64_t duration = static_cast<int64_t>(time(nullptr)) - m_transactionStartTime;
    if (duration >= LONG_TRANSACTION_WARNING_THRESHOLD_SECS) {
      CBL_WARNING << "Long transaction '" << m_transaction->name() << "': " << duration << " seconds";
    }
  }
  string errorMessage;
  int execResult = execManyInternal(commit ? "END TRANSACTION;" : "ROLLBACK;", errorMessage);
  Transaction* oldTransaction = m_transaction;
  m_transaction = nullptr;
  if (execResult != SQLITE_OK) {
    throwSqliteError(execResult, string("Failed to end transaction '") +
                                     (oldTransaction ? oldTransaction->name() : "") + "': " + errorMessage);
  }
}

void Database::setSynchronousMode(SynchronousMode mode) {
  const char* statement = "";
  switch (mode) {
    case SYNC_OFF:
      statement = "PRAGMA synchronous = OFF;";
      break;
    case SYNC_NORMAL:
      statement = "PRAGMA synchronous = NORMAL;";
      break;
    case SYNC_FULL:
      statement = "PRAGMA synchronous = FULL;";
      break;
    case SYNC_EXTRA:
      statement = "PRAGMA synchronous = EXTRA;";
      break;
  }
  execMany(statement, UNLOCKED);
}

int64_t Database::loadGlobalInt64(const char* name, int64_t defaultValue) {
  Statement statement = prepareAndBind("SELECT value FROM sqlitewrapper_table WHERE key = ?1;", name);
  return statement.step() ? statement.columnInt64(0) : defaultValue;
}

void Database::saveGlobalInt64(const char* name, int64_t value) {
  exec("INSERT OR REPLACE INTO sqlitewrapper_table (key, value) VALUES (?1, ?2);", name, value);
}

string Database::loadGlobalBlob(const char* name) {
  Statement statement = prepareAndBind("SELECT value FROM sqlitewrapper_table WHERE key = ?1;", name);
  string value;
  if (statement.step()) {
    int columnSize = statement.columnSize(0);
    value.assign(static_cast<const char*>(statement.columnBlob(0)), columnSize);
  }
  return value;
}

void Database::saveGlobalBlob(const char* name, const string& value) {
  Statement statement = prepare("INSERT OR REPLACE INTO sqlitewrapper_table (key, value) VALUES (?1, ?2);");
  statement.bind(1, name);
  statement.bindBlob(2, value);
  statement.step();
}

}  // namespace sqlite
