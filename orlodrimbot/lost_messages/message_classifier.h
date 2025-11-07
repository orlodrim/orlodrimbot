#ifndef MESSAGE_CLASSIFIER_H
#define MESSAGE_CLASSIFIER_H

#include <memory>
#include <string>
#include <string_view>
#include "cbl/llm_query.h"

struct MessageClassification {
  enum class Language {
    UNKNOWN,
    FRENCH,
    ENGLISH,
    OTHER,
  };
  Language llmLanguage = Language::UNKNOWN;

  enum class Category {
    UNKNOWN,
    WIKI_QUESTION,
    NON_WIKI_QUESTION,
    THANKS,
    ARTICLE_DRAFT,
    OTHER,
  };
  Category llmCategory = Category::UNKNOWN;
  bool llmBlocked = false;
  Category localModelCategory = Category::UNKNOWN;

  Category finalCategory() const { return llmCategory != Category::UNKNOWN ? llmCategory : localModelCategory; }
  bool categoryHasHighConfidence() const { return llmCategory != Category::UNKNOWN; }
  std::string debugString() const;
};

std::string_view getStringOfLanguage(MessageClassification::Language language);
std::string_view getStringOfCategory(MessageClassification::Category category);
MessageClassification::Language getLanguageOfString(std::string_view languageString);
MessageClassification::Category getCategoryOfString(std::string_view categoryString);

constexpr std::string_view DEFAULT_LOCAL_CLASSIFIER_COMMAND = "not_supported";

class MessageClassifier {
public:
  explicit MessageClassifier(std::string_view localClassifierCommand = DEFAULT_LOCAL_CLASSIFIER_COMMAND,
                             cbl::LLMClient* llmClient = nullptr);
  virtual ~MessageClassifier() = default;
  virtual MessageClassification classify(std::string_view message) const;
  void setPrintThought(bool value) { m_printThought = value; }

  static std::string_view getPrompt();

private:
  void callLocalClassifier(std::string_view message, MessageClassification& classification) const;
  void callLLM(std::string_view message, MessageClassification& classification) const;

  bool m_printThought = false;
  std::string m_localClassifierCommand;
  cbl::LLMClient* m_llmClient;
  std::unique_ptr<cbl::LLMClient> m_ownedLLMClient;
};

#endif
