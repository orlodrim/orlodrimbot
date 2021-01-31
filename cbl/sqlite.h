// C++ wrapper for sqlite.
// All statements must be part of a transaction (ReadTransaction or WriteTransaction).
// WriteTransaction are rolled back unless they are explicitly committed. In particular, exceptions that interrupt
// write transactions cause a rollback.
// Each database is created with a default key => value table that can be accessed with Database::loadGlobal* and
// Database::saveGlobal*.
//
// Examples
//
// Open a database, initializing it with a demo_table table if it does not exist yet:
//   Database db = Database::open("db.sqlite", OpenParams(), [](Database& db) {
//     db.exec("CREATE TABLE demo_table(key TEXT, value TEXT);");
//   });
//
// Add a row to demo_table:
//   WriteTransaction writeTransaction(db, CBL_HERE);
//   db.exec("INSERT INTO demo_table(key, value) VALUES (?1, ?2);", "some_key", "some_value");
//   writeTransaction.commit();
//
// Read a row from demo_table:
//   ReadTransaction readTransaction(db, CBL_HERE);
//   Statement statement = db.prepareAndBind("SELECT value FROM demo_table WHERE key = ?1;", "some_key");
//   if (statement.step()) {
//     std::cout << "value: " << statement.columnTextNotNull(0) << "\n";
//   }
#ifndef CBL_SQLITE_H
#define CBL_SQLITE_H

#include <sqlite3.h>
#include <cstdint>
#include <functional>
#include <string>
#include "error.h"

namespace sqlite {

class SqliteError : public cbl::Error {
public:
  using Error::Error;
};

class ConstraintError : public SqliteError {
public:
  using SqliteError::SqliteError;
};

class NotInTransactionError : public SqliteError {
public:
  using SqliteError::SqliteError;
};

class NestedTransactionsError : public SqliteError {
public:
  using SqliteError::SqliteError;
};

class PrimaryKeyConstraintError : public ConstraintError {
public:
  using ConstraintError::ConstraintError;
};

class UniqueConstraintError : public ConstraintError {
public:
  using ConstraintError::ConstraintError;
};

class BusyError : public SqliteError {
public:
  using SqliteError::SqliteError;
};

class ReadOnlyError : public SqliteError {
public:
  using SqliteError::SqliteError;
};

class Database;

// Use Database::prepare or Database::prepareAndBind to construct a Statement.
class Statement {
public:
  Statement();
  Statement(const Statement&) = delete;
  Statement(Statement&& statement);
  ~Statement();
  Statement& operator=(const Statement&) = delete;
  Statement& operator=(Statement&&);
  // Clears all parameter bindings and resets the state so that the next call to step() returns the first row.
  void reset();
  // Use parameter = 1 to set the value of ?1 in the statement, parameter = 2 for ?2, etc.
  void bind(int parameter, int value);
  void bind(int parameter, int64_t value);
  void bind(int parameter, double value);
  void bind(int parameter, const std::string& value) { bind(parameter, value.c_str()); }
  // Passing value = nullptr is allowed and equivalent to bindNull().
  void bind(int parameter, const char* value);
  void bindBlob(int parameter, const std::string& value) { bindBlob(parameter, value.c_str(), value.size()); }
  void bindBlob(int parameter, const char* value, int length);
  void bindNull(int parameter);
  template <typename... Args>
  void bindAll(const Args&... args) {
    bindFromIndex(1, args...);
  }

  // One step of statement evalatuation.
  // For a SELECT, this returns true if a row has been fetched or false if there are no more rows.
  // If it returns true, the data for the row can be accessed with the column* functions below.
  bool step();

  // For all column* functions below, column indexes are 0-based (for "SELECT x, y FROM ...", x is 0 and y is 1).
  // The result after a type conversion is undefined (https://www.sqlite.org/c3ref/column_blob.html), so this must not
  // called after any of the column* functions below.
  bool isColumnNull(int column) { return sqlite3_column_type(m_stmt, column) == SQLITE_NULL; }
  int columnInt(int column) { return sqlite3_column_int(m_stmt, column); }
  int64_t columnInt64(int column) { return sqlite3_column_int64(m_stmt, column); }
  double columnDouble(int column) { return sqlite3_column_double(m_stmt, column); }
  const char* columnText(int column) { return (const char*)sqlite3_column_text(m_stmt, column); }
  const char* columnTextNotNull(int column) {
    const char* text = columnText(column);
    return text ? text : "";
  }
  const void* columnBlob(int column) { return sqlite3_column_blob(m_stmt, column); }
  int columnSize(int column) { return sqlite3_column_bytes(m_stmt, column); }

private:
  Statement(Database* database, const std::string& text);

  void bindFromIndex(int index);
  template <typename T, typename... Args>
  void bindFromIndex(int index, const T& value, const Args&... args) {
    bind(index, value);
    bindFromIndex(index + 1, args...);
  }

  sqlite3_stmt* m_stmt = nullptr;
  Database* m_database = nullptr;
  std::string m_text;
  int m_numParameters = 0;
  bool m_requiresWriteLock = false;

  friend class Database;
};

class Transaction {
public:
  Transaction(const Transaction&) = delete;
  ~Transaction();
  void operator=(const Transaction&) = delete;

  bool exclusive() const { return m_exclusive; }
  const char* name() const { return m_name; }

protected:
  Transaction(Database& database, bool exclusive, const char* name);

  Database& m_database;
  bool m_exclusive;
  const char* m_name;
};

// Within a ReadTransaction, only SELECT statements can be executed.
class ReadTransaction : public Transaction {
public:
  ReadTransaction(Database& database, const char* name) : Transaction(database, false, name) {}
};

// Within a WriteTransaction, all statements are allowed.
// commit() must be called after executing statements. Otherwise, the destructor rolls back to the state before the
// transaction.
class WriteTransaction : public Transaction {
public:
  WriteTransaction(Database& database, const char* name) : Transaction(database, true, name) {}

  void commit();
};

enum OpenMode {
  OPEN_READONLY,
  OPEN_READWRITE,
  OPEN_OR_CREATE,
};

enum SynchronousMode {
  SYNC_OFF,
  SYNC_NORMAL,
  SYNC_FULL,
  SYNC_EXTRA,
};

enum LockType {
  UNLOCKED,
  READ_LOCK,
  WRITE_LOCK,
};

struct OpenParams {
  OpenMode openMode = OPEN_OR_CREATE;
  SynchronousMode synchronousMode = SYNC_FULL;
};

class Database {
public:
  using InitCallback = std::function<void(Database&)>;
  [[nodiscard]] static Database open(const std::string& path, const OpenParams& params = {},
                                     const InitCallback& initCallback = {});

  Database();
  Database(const Database&) = delete;
  Database(Database&& database);
  ~Database();
  Database& operator=(const Database&) = delete;
  Database& operator=(Database&& database);

  Statement prepare(const std::string& text) { return Statement(this, text); }
  template <typename... Args>
  Statement prepareAndBind(const std::string& text, const Args&... args) {
    Statement statement = prepare(text);
    statement.bindAll(args...);
    return statement;
  }
  template <typename... Args>
  void exec(const std::string& statement, const Args&... args) {
    prepareAndBind(statement, args...).step();
  }
  void execMany(const std::string& statement, LockType requiredLock = WRITE_LOCK);
  int64_t lastInsertRowid() const { return sqlite3_last_insert_rowid(m_db); }
  void setSynchronousMode(SynchronousMode value);

  int64_t loadGlobalInt64(const char* name, int64_t defaultValue);
  void saveGlobalInt64(const char* name, int64_t value);
  std::string loadGlobalBlob(const char* name);
  void saveGlobalBlob(const char* name, const std::string& value);

  static void forceSynchronousModeOff() { synchronousModeForcedOff = true; }

private:
  // Should not be called as part of the constructor, so that ~Database() is called if it throws.
  void openInternal(const std::string& path, const OpenParams& params, const InitCallback& initCallback);
  void close();
  // Does not check locks.
  int execManyInternal(const std::string& statement, std::string& errorMessage);
  std::string lastErrorMessage() const;
  void beginTransaction(Transaction* transaction);
  void endTransaction(bool commit);

  sqlite3* m_db = nullptr;
  Transaction* m_transaction = nullptr;

  static bool synchronousModeForcedOff;

  friend class Statement;
  friend class Transaction;
  friend class WriteTransaction;
};

}  // namespace sqlite

#endif
