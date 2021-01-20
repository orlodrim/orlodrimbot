#include "date_parser.h"
#include <re2/re2.h>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include "cbl/date.h"
#include "cbl/unicode_fr.h"

using cbl::Date;
using cbl::DateDiff;
using std::string;
using std::string_view;
using std::unordered_map;

namespace wikiutil {
namespace {

constexpr int DAYS_PER_MONTH[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

int getNumDaysInMonth(int month, int year) {
  if (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) {
    return 29;
  } else if (month >= 1 && month <= 12) {
    return DAYS_PER_MONTH[month];
  } else {
    return 0;
  }
}

int getMonthIndex(string_view monthName) {
  static const unordered_map<string, int> MONTHS_BY_NAME = {
      {"janvier", 1},  {"février", 2},   {"mars", 3},      {"avril", 4}, {"mai", 5},
      {"juin", 6},     {"juillet", 7},   {"août", 8},      {"aout", 8},  {"septembre", 9},
      {"octobre", 10}, {"novembre", 11}, {"décembre", 12},
  };
  string lowerCaseWord = unicode_fr::toLowerCase(monthName);
  unordered_map<string, int>::const_iterator monthIt = MONTHS_BY_NAME.find(lowerCaseWord);
  return monthIt != MONTHS_BY_NAME.end() ? monthIt->second : 0;
}

class Lexer {
public:
  enum TokenType {
    OTHER,
    DAY,               // Day (e.g. "1er").
    MONTH,             // Month name.
    YEAR,              // Year, with at least 3 digits.
    TWO_DIGIT_NUMBER,  // Day, month number or year.
    END,
  };
  explicit Lexer(string_view text) : m_text(text.data(), text.size()) { next(); }

  void next();
  void saveState();
  void restoreStateAndNext();

  string_view token() const { return m_token; }
  TokenType tokenType() const { return m_tokenType; }
  int tokenValue() const { return m_tokenValue; }

  bool consumeValue(TokenType expectedType, int& value);
  bool consumeString(string_view str);

private:
  re2::StringPiece m_text;
  string_view m_token;
  TokenType m_tokenType = OTHER;
  int m_tokenValue = 0;

  re2::StringPiece m_savedText;
};

void Lexer::next() {
  static const re2::RE2 reSpace(R"(\s*)");
  static const re2::RE2 reToken(R"((1(?:[Ee][Rr]\b|\{\{[Ee]r\}\})|\{\{1er\}\})|(\d{1,4}\b)|([\pL\pN]+\b)|(.))");
  RE2::Consume(&m_text, reSpace);
  const char* tokenStart = m_text.data();
  re2::StringPiece number, number1, word, otherChar;
  if (!RE2::Consume(&m_text, reToken, &number1, &number, &word, &otherChar)) {
    m_tokenType = END;
    return;
  }
  m_tokenType = OTHER;
  m_tokenValue = 0;
  if (!number1.empty()) {
    m_tokenValue = 1;
    m_tokenType = DAY;
  } else if (!number.empty()) {
    for (char c : number) {
      m_tokenValue = m_tokenValue * 10 + c - '0';
    }
    m_tokenType = number.size() <= 2 ? TWO_DIGIT_NUMBER : YEAR;
  } else if (!word.empty()) {
    m_tokenValue = getMonthIndex(string_view(word.data(), word.size()));
    m_tokenType = m_tokenValue != 0 ? MONTH : OTHER;
  }
  m_token = string_view(tokenStart, m_text.data() - tokenStart);
}

void Lexer::saveState() {
  m_savedText = m_text;
}

void Lexer::restoreStateAndNext() {
  m_text = m_savedText;
  next();
}

bool Lexer::consumeValue(TokenType expectedType, int& value) {
  if (m_tokenType != expectedType) return false;
  value = m_tokenValue;
  next();
  return true;
}

bool Lexer::consumeString(string_view expectedString) {
  if (m_tokenType != OTHER || m_token != expectedString) return false;
  next();
  return true;
}

class FrenchDateParser : public DateParser {
public:
  SignatureDate extractFirstSignatureDate(string_view text) const override;
  SignatureDate extractMaxSignatureDate(string_view text) const override;
  Date parseDate(string_view text, int flags) const override;
  Date extractFirstDate(string_view text, int flags) const override;

private:
  static bool jumpToNextValidDay(Lexer& lexer);
  static bool consumeSignatureDate(Lexer& lexer, SignatureDate& date);
  static SignatureDate findAndConsumeSignatureDate(Lexer& lexer);
  static bool consumeDate(Lexer& lexer, Date& date, int flags);
};

bool FrenchDateParser::jumpToNextValidDay(Lexer& lexer) {
  for (; lexer.tokenType() != Lexer::END; lexer.next()) {
    if ((lexer.tokenType() == Lexer::DAY || lexer.tokenType() == Lexer::TWO_DIGIT_NUMBER) && lexer.tokenValue() >= 1 &&
        lexer.tokenValue() <= 31) {
      return true;
    }
  }
  return false;
}

bool FrenchDateParser::consumeSignatureDate(Lexer& lexer, SignatureDate& date) {
  int day = 0, month = 0, year = 0, hour = 0, minute = 0;
  if (!(lexer.consumeValue(Lexer::TWO_DIGIT_NUMBER, day) && day >= 1 && day <= 31)) return false;
  if (!lexer.consumeValue(Lexer::MONTH, month)) return false;
  if (!(lexer.consumeValue(Lexer::YEAR, year) && year >= 2000)) return false;
  if (day > getNumDaysInMonth(month, year)) return false;
  if (!lexer.consumeString("à")) return false;
  if (!(lexer.consumeValue(Lexer::TWO_DIGIT_NUMBER, hour) && hour >= 0 && hour < 24)) return false;
  if (!lexer.consumeString(":")) return false;
  if (!(lexer.consumeValue(Lexer::TWO_DIGIT_NUMBER, minute) && minute >= 0 && minute < 60)) return false;
  date.localTimeDiff = DateDiff(0);
  if (lexer.consumeString("(")) {
    if (lexer.consumeString("CET") && lexer.consumeString(")")) {
      date.localTimeDiff = DateDiff(3600);
    } else if (lexer.consumeString("CEST") && lexer.consumeString(")")) {
      date.localTimeDiff = DateDiff(3600 * 2);
    }
  }
  date.utcDate = Date(year, month, day, hour, minute) - date.localTimeDiff;
  // Reject dates in the future with some margin (2 hours in case the time zone is not read correctly + 5 minutes of
  // tolerance on the computer clock).
  return date.utcDate < Date::now() + DateDiff(3600 * 2 + 300);
}

SignatureDate FrenchDateParser::findAndConsumeSignatureDate(Lexer& lexer) {
  SignatureDate date;
  while (jumpToNextValidDay(lexer)) {
    lexer.saveState();
    if (consumeSignatureDate(lexer, date)) {
      return date;
    }
    lexer.restoreStateAndNext();
  }
  return SignatureDate();
}

bool FrenchDateParser::consumeDate(Lexer& lexer, Date& date, int flags) {
  int day = 0, month = 0, year = 0;
  bool simpleDay = lexer.tokenType() == Lexer::TWO_DIGIT_NUMBER;
  if (!(lexer.consumeValue(Lexer::TWO_DIGIT_NUMBER, day) && day >= 1 && day <= 31) &&
      !lexer.consumeValue(Lexer::DAY, day)) {
    return false;
  }
  if (lexer.consumeValue(Lexer::MONTH, month)) {
    if ((lexer.consumeValue(Lexer::TWO_DIGIT_NUMBER, year) || lexer.consumeValue(Lexer::YEAR, year)) && year >= 1) {
      // year is now set.
    } else if (flags & IMPLICIT_YEAR) {
      Date minDate = Date::now() - DateDiff(3600 * 24 * 270);
      year = minDate.year() + (month <= minDate.month() ? 1 : 0);
    } else {
      return false;
    }
  } else if ((flags & ALLOW_NUMERIC_MONTH) && simpleDay && lexer.consumeString("/")) {
    if (!(lexer.consumeValue(Lexer::TWO_DIGIT_NUMBER, month) && month >= 1 && month <= 12)) return false;
    if (!lexer.consumeString("/")) return false;
    if (lexer.consumeValue(Lexer::TWO_DIGIT_NUMBER, year)) {
      int minYear = Date::now().year() - 80;
      year += (minYear / 100) * 100;
      if (year < minYear) year += 100;
    } else if (!(lexer.consumeValue(Lexer::YEAR, year) && year >= 1)) {
      return false;
    }
  } else {
    return false;
  }
  if ((flags & AFTER_2000) && year < 2000) return false;
  if (day > getNumDaysInMonth(month, year)) return false;
  int maxSecondsInTheFuture = 3600 * 2 + 300;  // Time zone + clock error.
  if (flags & END_OF_DAY) {
    date = Date(year, month, day, 23, 59, 59);
    maxSecondsInTheFuture += 3600 * 24;
  } else {
    date = Date(year, month, day, 0, 0, 0);
  }
  if ((flags & BEFORE_NOW) && date >= Date::now() + DateDiff(maxSecondsInTheFuture)) return false;
  return true;
}

SignatureDate FrenchDateParser::extractFirstSignatureDate(string_view text) const {
  Lexer lexer(text);
  return findAndConsumeSignatureDate(lexer);
}

SignatureDate FrenchDateParser::extractMaxSignatureDate(string_view text) const {
  Lexer lexer(text);
  SignatureDate date;
  while (true) {
    SignatureDate otherDate = findAndConsumeSignatureDate(lexer);
    if (otherDate.isNull()) break;
    if (otherDate.utcDate > date.utcDate) {
      date = otherDate;
    }
  }
  return date;
}

Date FrenchDateParser::parseDate(string_view text, int flags) const {
  Lexer lexer(text);
  Date date;
  return consumeDate(lexer, date, flags) && lexer.tokenType() == Lexer::END ? date : Date();
}

Date FrenchDateParser::extractFirstDate(string_view text, int flags) const {
  Lexer lexer(text);
  Date date;
  while (jumpToNextValidDay(lexer)) {
    lexer.saveState();
    if (consumeDate(lexer, date, flags)) {
      return date;
    }
    lexer.restoreStateAndNext();
  }
  return Date();
}

}  // namespace

const DateParser& DateParser::getByLang(std::string_view lang) {
  if (lang == "fr") {
    static const FrenchDateParser frenchDateParser;
    return frenchDateParser;
  }
  throw std::invalid_argument("Unsupported lang passed to DateParser::getByLang");
}

}  // namespace wikiutil
