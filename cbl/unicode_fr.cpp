#include "unicode_fr.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include "utf8.h"

using cbl::utf8::consumeChar;
using cbl::utf8::encode;
using cbl::utf8::EncodeBuffer;
using std::pair;
using std::string;
using std::string_view;
using std::unordered_map;

namespace unicode_fr {
namespace {

struct RangeMapping {
  int start;
  int size;
  int step;
  int delta;
};

struct CaseData {
  const pair<int, int>* simpleMap;
  const RangeMapping* rangeMap;
  int simpleMapSize;
  int rangeMapSize;
};

constexpr pair<int, int> LOWER_CASE_SIMPLE_MAP[] = {
    {304, 105},    {376, 255},     {385, 595},   {390, 596},     {391, 392},     {395, 396},     {398, 477},
    {399, 601},    {400, 603},     {401, 402},   {403, 608},     {404, 611},     {406, 617},     {407, 616},
    {408, 409},    {412, 623},     {413, 626},   {415, 629},     {422, 640},     {423, 424},     {425, 643},
    {428, 429},    {430, 648},     {431, 432},   {439, 658},     {440, 441},     {444, 445},     {452, 454},
    {453, 454},    {455, 457},     {456, 457},   {458, 460},     {497, 499},     {502, 405},     {503, 447},
    {544, 414},    {570, 11365},   {571, 572},   {573, 410},     {574, 11366},   {577, 578},     {579, 384},
    {580, 649},    {581, 652},     {886, 887},   {895, 1011},    {902, 940},     {908, 972},     {975, 983},
    {1012, 952},   {1015, 1016},   {1017, 1010}, {1018, 1019},   {1216, 1231},   {4295, 11559},  {4301, 11565},
    {7838, 223},   {8124, 8115},   {8140, 8131}, {8172, 8165},   {8188, 8179},   {8486, 969},    {8490, 107},
    {8491, 229},   {8498, 8526},   {8579, 8580}, {11360, 11361}, {11362, 619},   {11363, 7549},  {11364, 637},
    {11373, 593},  {11374, 625},   {11375, 592}, {11376, 594},   {11378, 11379}, {11381, 11382}, {11506, 11507},
    {42877, 7545}, {42891, 42892}, {42893, 613}, {42922, 614},   {42923, 604},   {42924, 609},   {42925, 620},
    {42926, 618},  {42928, 670},   {42929, 647}, {42930, 669},   {42931, 43859},
};
constexpr RangeMapping LOWER_CASE_RANGE_MAP[] = {
    {65, 26, 1, 32},    {192, 23, 1, 32},      {216, 7, 1, 32},    {256, 24, 2, 1},     {306, 3, 2, 1},
    {313, 8, 2, 1},     {330, 23, 2, 1},       {377, 3, 2, 1},     {386, 2, 2, 1},      {393, 2, 1, 205},
    {416, 3, 2, 1},     {433, 2, 1, 217},      {435, 2, 2, 1},     {459, 9, 2, 1},      {478, 9, 2, 1},
    {498, 2, 2, 1},     {504, 20, 2, 1},       {546, 9, 2, 1},     {582, 5, 2, 1},      {880, 2, 2, 1},
    {904, 3, 1, 37},    {910, 2, 1, 63},       {913, 17, 1, 32},   {931, 9, 1, 32},     {984, 12, 2, 1},
    {1021, 3, 1, -130}, {1024, 16, 1, 80},     {1040, 32, 1, 32},  {1120, 17, 2, 1},    {1162, 27, 2, 1},
    {1217, 7, 2, 1},    {1232, 48, 2, 1},      {1329, 38, 1, 48},  {4256, 38, 1, 7264}, {5024, 80, 1, 38864},
    {5104, 6, 1, 8},    {7680, 75, 2, 1},      {7840, 48, 2, 1},   {7944, 8, 1, -8},    {7960, 6, 1, -8},
    {7976, 8, 1, -8},   {7992, 8, 1, -8},      {8008, 6, 1, -8},   {8025, 4, 2, -8},    {8040, 8, 1, -8},
    {8072, 8, 1, -8},   {8088, 8, 1, -8},      {8104, 8, 1, -8},   {8120, 2, 1, -8},    {8122, 2, 1, -74},
    {8136, 4, 1, -86},  {8152, 2, 1, -8},      {8154, 2, 1, -100}, {8168, 2, 1, -8},    {8170, 2, 1, -112},
    {8184, 2, 1, -128}, {8186, 2, 1, -126},    {8544, 16, 1, 16},  {9398, 26, 1, 26},   {11264, 47, 1, 48},
    {11367, 3, 2, 1},   {11390, 2, 1, -10815}, {11392, 50, 2, 1},  {11499, 2, 2, 1},    {42560, 23, 2, 1},
    {42624, 14, 2, 1},  {42786, 7, 2, 1},      {42802, 31, 2, 1},  {42873, 2, 2, 1},    {42878, 5, 2, 1},
    {42896, 2, 2, 1},   {42902, 10, 2, 1},     {42932, 2, 2, 1},   {65313, 26, 1, 32},  {66560, 40, 1, 40},
    {66736, 36, 1, 40}, {68736, 51, 1, 64},    {71840, 32, 1, 32}, {125184, 34, 1, 34},
};
constexpr CaseData LOWER_CASE_DATA = {LOWER_CASE_SIMPLE_MAP, LOWER_CASE_RANGE_MAP, 89, 79};

constexpr pair<int, int> UPPER_CASE_SIMPLE_MAP[] = {
    {181, 924},     {255, 376},     {305, 73},     {383, 83},      {384, 579},     {392, 391},     {396, 395},
    {402, 401},     {405, 502},     {409, 408},    {410, 573},     {414, 544},     {424, 423},     {429, 428},
    {432, 431},     {441, 440},     {445, 444},    {447, 503},     {453, 452},     {454, 452},     {456, 455},
    {457, 455},     {459, 458},     {460, 458},    {477, 398},     {498, 497},     {499, 497},     {501, 500},
    {572, 571},     {578, 577},     {592, 11375},  {593, 11373},   {594, 11376},   {595, 385},     {596, 390},
    {601, 399},     {603, 400},     {604, 42923},  {608, 403},     {609, 42924},   {611, 404},     {613, 42893},
    {614, 42922},   {616, 407},     {617, 406},    {618, 42926},   {619, 11362},   {620, 42925},   {623, 412},
    {625, 11374},   {626, 413},     {629, 415},    {637, 11364},   {640, 422},     {643, 425},     {647, 42929},
    {648, 430},     {649, 580},     {652, 581},    {658, 439},     {669, 42930},   {670, 42928},   {837, 921},
    {887, 886},     {940, 902},     {962, 931},    {972, 908},     {976, 914},     {977, 920},     {981, 934},
    {982, 928},     {983, 975},     {1008, 922},   {1009, 929},    {1010, 1017},   {1011, 895},    {1013, 917},
    {1016, 1015},   {1019, 1018},   {1231, 1216},  {7296, 1042},   {7297, 1044},   {7298, 1054},   {7301, 1058},
    {7302, 1066},   {7303, 1122},   {7304, 42570}, {7545, 42877},  {7549, 11363},  {7835, 7776},   {8126, 921},
    {8165, 8172},   {8526, 8498},   {8580, 8579},  {11361, 11360}, {11365, 570},   {11366, 574},   {11379, 11378},
    {11382, 11381}, {11507, 11506}, {11559, 4295}, {11565, 4301},  {42892, 42891}, {43859, 42931},
};
constexpr RangeMapping UPPER_CASE_RANGE_MAP[] = {
    {97, 26, 1, -32},     {224, 23, 1, -32},   {248, 7, 1, -32},    {257, 24, 2, -1},    {307, 3, 2, -1},
    {314, 8, 2, -1},      {331, 23, 2, -1},    {378, 3, 2, -1},     {387, 2, 2, -1},     {417, 3, 2, -1},
    {436, 2, 2, -1},      {462, 8, 2, -1},     {479, 9, 2, -1},     {505, 20, 2, -1},    {547, 9, 2, -1},
    {575, 2, 1, 10815},   {583, 5, 2, -1},     {598, 2, 1, -205},   {650, 2, 1, -217},   {881, 2, 2, -1},
    {891, 3, 1, 130},     {941, 3, 1, -37},    {945, 17, 1, -32},   {963, 9, 1, -32},    {973, 2, 1, -63},
    {985, 12, 2, -1},     {1072, 32, 1, -32},  {1104, 16, 1, -80},  {1121, 17, 2, -1},   {1163, 27, 2, -1},
    {1218, 7, 2, -1},     {1233, 48, 2, -1},   {1377, 38, 1, -48},  {5112, 6, 1, -8},    {7299, 2, 1, -6242},
    {7681, 75, 2, -1},    {7841, 48, 2, -1},   {7936, 8, 1, 8},     {7952, 6, 1, 8},     {7968, 8, 1, 8},
    {7984, 8, 1, 8},      {8000, 6, 1, 8},     {8017, 4, 2, 8},     {8032, 8, 1, 8},     {8048, 2, 1, 74},
    {8050, 4, 1, 86},     {8054, 2, 1, 100},   {8056, 2, 1, 128},   {8058, 2, 1, 112},   {8060, 2, 1, 126},
    {8112, 2, 1, 8},      {8144, 2, 1, 8},     {8160, 2, 1, 8},     {8560, 16, 1, -16},  {9424, 26, 1, -26},
    {11312, 47, 1, -48},  {11368, 3, 2, -1},   {11393, 50, 2, -1},  {11500, 2, 2, -1},   {11520, 38, 1, -7264},
    {42561, 23, 2, -1},   {42625, 14, 2, -1},  {42787, 7, 2, -1},   {42803, 31, 2, -1},  {42874, 2, 2, -1},
    {42879, 5, 2, -1},    {42897, 2, 2, -1},   {42903, 10, 2, -1},  {42933, 2, 2, -1},   {43888, 80, 1, -38864},
    {65345, 26, 1, -32},  {66600, 40, 1, -40}, {66776, 36, 1, -40}, {68800, 51, 1, -64}, {71872, 32, 1, -32},
    {125218, 34, 1, -34},
};
constexpr CaseData UPPER_CASE_DATA = {UPPER_CASE_SIMPLE_MAP, UPPER_CASE_RANGE_MAP, 104, 76};

constexpr pair<int, int> TITLE_CASE_SIMPLE_MAP[] = {
    {452, 453}, {453, 453}, {454, 453}, {455, 456}, {456, 456},   {457, 456},   {458, 459},   {459, 459},
    {460, 459}, {497, 498}, {498, 498}, {499, 498}, {8115, 8124}, {8131, 8140}, {8179, 8188},
};
constexpr RangeMapping TITLE_CASE_RANGE_MAP[] = {
    {8064, 8, 1, 8},
    {8080, 8, 1, 8},
    {8096, 8, 1, 8},
};
constexpr CaseData TITLE_CASE_DATA = {TITLE_CASE_SIMPLE_MAP, TITLE_CASE_RANGE_MAP, 15, 3};

void fillCaseMap(const CaseData& caseData, unordered_map<int, int>& result) {
  for (int i = 0; i < caseData.simpleMapSize; i++) {
    result[caseData.simpleMap[i].first] = caseData.simpleMap[i].second;
  }
  for (int i = 0; i < caseData.rangeMapSize; i++) {
    const RangeMapping& rangeMapping = caseData.rangeMap[i];
    int c = rangeMapping.start;
    for (int j = 0; j < rangeMapping.size; j++) {
      result[c] = c + rangeMapping.delta;
      c += rangeMapping.step;
    }
  }
}

unordered_map<int, int> initCaseMap(const CaseData& caseData) {
  unordered_map<int, int> result;
  fillCaseMap(caseData, result);
  return result;
}

}  // namespace

string_view charToLowerCase(int c, EncodeBuffer& buffer) {
  static const unordered_map<int, int> lowerCaseMap = initCaseMap(LOWER_CASE_DATA);
  unordered_map<int, int>::const_iterator it = lowerCaseMap.find(c);
  return encode(it == lowerCaseMap.end() ? c : it->second, buffer);
}

string_view charToUpperCase(int c, EncodeBuffer& buffer) {
  static const unordered_map<int, int> upperCaseMap = initCaseMap(UPPER_CASE_DATA);
  unordered_map<int, int>::const_iterator it = upperCaseMap.find(c);
  return encode(it == upperCaseMap.end() ? c : it->second, buffer);
}

string_view charToTitleCase(int c, EncodeBuffer& buffer) {
  static const unordered_map<int, int> titleCaseMap = []() {
    unordered_map<int, int> result = initCaseMap(UPPER_CASE_DATA);
    fillCaseMap(TITLE_CASE_DATA, result);
    return result;
  }();
  unordered_map<int, int>::const_iterator it = titleCaseMap.find(c);
  return encode(it == titleCaseMap.end() ? c : it->second, buffer);
}

string toLowerCase(string_view text) {
  string newText;
  newText.reserve(text.size());
  string_view partToConvert = text;
  while (true) {
    int c = consumeChar(partToConvert);
    if (c == -1) break;  // Invalid character.
    EncodeBuffer encodeBuffer;
    newText += charToLowerCase(c, encodeBuffer);
  }
  return newText;
}

string toUpperCase(string_view text) {
  string newText;
  newText.reserve(text.size());
  string_view partToConvert = text;
  while (true) {
    int c = consumeChar(partToConvert);
    if (c == -1) break;  // Invalid character.
    EncodeBuffer encodeBuffer;
    newText += charToUpperCase(c, encodeBuffer);
  }
  return newText;
}

string capitalize(string_view text) {
  string newText;
  string_view textWithoutFirstChar = text;
  int firstChar = consumeChar(textWithoutFirstChar);
  if (firstChar != -1) {
    EncodeBuffer encodeBuffer;
    string_view encodedFirstChar = charToTitleCase(firstChar, encodeBuffer);
    newText.reserve(encodedFirstChar.size() + textWithoutFirstChar.size());
    newText += encodedFirstChar;
    newText += textWithoutFirstChar;
  } else {
    newText = text;
  }
  return newText;
}

}  // namespace unicode_fr
