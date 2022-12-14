// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides Japanese phonetic disambiguation data for ChromeVox.
 */

goog.provide('JaPhoneticData');

JaPhoneticData = class {
  constructor() {}

  /**
   * Initialize phoneticMap_ by |map|.
   * @param {Map<string, string>} map
   */
  static init(map) {
    /**
     * A map containing phonetic disambiguation data for Japanese.
     * @private {Map<string, string>}
     */
    this.phoneticMap_ = map;
  }

  /**
   * Returns a phonetic reading for |char|.
   * @param {string} char
   * @return {string}
   */
  static forCharacter(char) {
    const characterSet =
        JaPhoneticData.getCharacterSet(char, JaPhoneticData.CharacterSet.NONE);
    let resultChar = JaPhoneticData.maybeGetLargeLetterKana(char);
    resultChar = JaPhoneticData.phoneticMap_.get(resultChar) || resultChar;
    const prefix = JaPhoneticData.getPrefixForCharacter(characterSet);
    if (prefix) {
      return prefix + ' ' + resultChar;
    }
    return resultChar;
  }

  /**
   * Returns a phonetic reading for |text|.
   * @param {string} text
   * @return {string}
   */
  static forText(text) {
    const result = [];
    const chars = [...text];
    let lastCharacterSet = JaPhoneticData.CharacterSet.NONE;
    for (const char of chars) {
      const currentCharacterSet =
          JaPhoneticData.getCharacterSet(char, lastCharacterSet);
      const info =
          JaPhoneticData.getPrefixInfo(lastCharacterSet, currentCharacterSet);
      if (info.prefix) {
        // Need to announce the new character set explicitly.
        result.push(info.prefix);
      }

      if (info.delimiter === false && result.length > 0) {
        // Does not convert small Kana if it is not the beginning of the
        // element.
        result[result.length - 1] += char;
      } else if (JaPhoneticData.alwaysReadPhonetically(currentCharacterSet)) {
        result.push(JaPhoneticData.phoneticMap_.get(char) || char);
      } else {
        result.push(JaPhoneticData.maybeGetLargeLetterKana(char));
      }

      lastCharacterSet = currentCharacterSet;
    }
    return result.join(' ');
  }

  /**
   * @param {string} character
   * @param {JaPhoneticData.CharacterSet} lastCharacterSet
   * @return {JaPhoneticData.CharacterSet}
   */
  static getCharacterSet(character, lastCharacterSet) {
    // See https://www.unicode.org/charts/PDF/U3040.pdf
    if (character >= '???' && character <= '???') {
      if (JaPhoneticData.isSmallLetter(character)) {
        return JaPhoneticData.CharacterSet.HIRAGANA_SMALL_LETTER;
      }
      return JaPhoneticData.CharacterSet.HIRAGANA;
    }
    // See https://www.unicode.org/charts/PDF/U30A0.pdf
    if (character >= '???' && character <= '???') {
      if (JaPhoneticData.isSmallLetter(character)) {
        return JaPhoneticData.CharacterSet.KATAKANA_SMALL_LETTER;
      }
      return JaPhoneticData.CharacterSet.KATAKANA;
    }
    // See https://unicode.org/charts/PDF/UFF00.pdf
    if (character >= '???' && character <= '???') {
      if (JaPhoneticData.isSmallLetter(character)) {
        return JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA_SMALL_LETTER;
      }
      return JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA;
    }
    if (character >= 'A' && character <= 'Z') {
      return JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_UPPER;
    }
    if (character >= 'a' && character <= 'z') {
      return JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_LOWER;
    }
    if (character >= '0' && character <= '9') {
      return JaPhoneticData.CharacterSet.HALF_WIDTH_NUMERIC;
    }
    // See https://unicode.org/charts/PDF/U0000.pdf
    if (character >= '!' && character <= '~') {
      return JaPhoneticData.CharacterSet.HALF_WIDTH_SYMBOL;
    }
    if (character >= '???' && character <= '???') {
      return JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER;
    }
    if (character >= '???' && character <= '???') {
      return JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER;
    }
    if (character >= '???' && character <= '???') {
      return JaPhoneticData.CharacterSet.FULL_WIDTH_NUMERIC;
    }
    // See https://unicode.org/charts/PDF/UFF00.pdf
    if (character >= '???' && character <= '???') {
      return JaPhoneticData.CharacterSet.FULL_WIDTH_SYMBOL;
    }
    if (character === '???') {
      switch (lastCharacterSet) {
        case JaPhoneticData.CharacterSet.HIRAGANA:
        case JaPhoneticData.CharacterSet.KATAKANA:
        case JaPhoneticData.CharacterSet.HIRAGANA_SMALL_LETTER:
        case JaPhoneticData.CharacterSet.KATAKANA_SMALL_LETTER:
          return lastCharacterSet;
      }
    }
    // See https://www.unicode.org/charts/PDF/U0400.pdf and
    // https://www.unicode.org/charts/PDF/U0370.pdf
    if ((character >= '??' && character <= '??') ||
        (character >= '??' && character <= '??')) {
      return JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_UPPER;
    }
    if ((character >= '??' && character <= '??') ||
        (character >= '??' && character <= '??')) {
      return JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_LOWER;
    }
    // Returns OTHER for all other characters, including Kanji.
    return JaPhoneticData.CharacterSet.OTHER;
  }

  /**
   * Returns true if the character is part of the small letter character set.
   * @param {string} character
   * @return {boolean}
   */
  static isSmallLetter(character) {
    return JaPhoneticData.SMALL_TO_LARGE.has(character);
  }

  /**
   * Returns a large equivalent if the character is a small letter of Kana.
   * @param {string} character
   * @return {string}
   */
  static maybeGetLargeLetterKana(character) {
    return JaPhoneticData.SMALL_TO_LARGE.get(character) || character;
  }

  /**
   * @param {JaPhoneticData.CharacterSet} characterSet
   * @return {string}
   */
  static getDefaultPrefix(characterSet) {
    return JaPhoneticData.DEFAULT_PREFIX.get(characterSet);
  }

  /**
   * @param {JaPhoneticData.CharacterSet} characterSet
   * @return {?string}
   */
  static getPrefixForCharacter(characterSet) {
    // Removing an annoucement of capital because users can distinguish
    // uppercase and lowercase by capiatalStrategy options.
    switch (characterSet) {
      case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_UPPER:
      case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_LOWER:
      case JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_UPPER:
      case JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_LOWER:
      case JaPhoneticData.CharacterSet.OTHER:
        return null;
      case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER:
        return '????????????';
    }
    return JaPhoneticData.getDefaultPrefix(characterSet);
  }

  /**
   * Returns an object containing the relationship between the preceding
   * character set and the current character set.
   * @param {JaPhoneticData.CharacterSet} lastCharacterSet
   * @param {JaPhoneticData.CharacterSet} currentCharacterSet
   * @return {Object<{delimiter: boolean, prefix: ?string}>}
   * delimiter: true if a space between preceding character and current
   * character is necessary. A space leaves a pause so users can recognize that
   * the type of characters has changed.
   * prefix: a string that represents the character set. Null if unncessary.
   */
  static getPrefixInfo(lastCharacterSet, currentCharacterSet) {
    // Don't add prefixes for the same character set except for the sets always
    // read phonetically.
    if (lastCharacterSet === currentCharacterSet) {
      return JaPhoneticData.alwaysReadPhonetically(currentCharacterSet) ?
          {delimiter: true, prefix: null} :
          {delimiter: false, prefix: null};
    }
    // Exceptional cases:
    switch (currentCharacterSet) {
      case JaPhoneticData.CharacterSet.HIRAGANA:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.NONE:
          case JaPhoneticData.CharacterSet.HIRAGANA_SMALL_LETTER:
            return {delimiter: false, prefix: null};
          case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_UPPER:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_LOWER:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_NUMERIC:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_NUMERIC:
          case JaPhoneticData.CharacterSet.OTHER:
            return {delimiter: true, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.KATAKANA:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.KATAKANA_SMALL_LETTER:
            return {delimiter: false, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.HIRAGANA_SMALL_LETTER:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.NONE:
          case JaPhoneticData.CharacterSet.HIRAGANA:
            return {delimiter: false, prefix: null};
          case JaPhoneticData.CharacterSet.OTHER:
            return {delimiter: true, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.KATAKANA_SMALL_LETTER:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.KATAKANA:
            return {delimiter: false, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA_SMALL_LETTER:
            return {delimiter: false, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA_SMALL_LETTER:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA:
            return {delimiter: false, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_UPPER:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_NUMERIC:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_SYMBOL:
            return {delimiter: true, prefix: '????????????????????????'};
        }
        break;
      case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_LOWER:
      case JaPhoneticData.CharacterSet.HALF_WIDTH_NUMERIC:
      case JaPhoneticData.CharacterSet.HALF_WIDTH_SYMBOL:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.NONE:
            return {delimiter: false, prefix: null};
          case JaPhoneticData.CharacterSet.HIRAGANA:
          case JaPhoneticData.CharacterSet.KATAKANA:
          case JaPhoneticData.CharacterSet.HIRAGANA_SMALL_LETTER:
          case JaPhoneticData.CharacterSet.KATAKANA_SMALL_LETTER:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA_SMALL_LETTER:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_UPPER:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_LOWER:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_NUMERIC:
          case JaPhoneticData.CharacterSet.HALF_WIDTH_SYMBOL:
          case JaPhoneticData.CharacterSet.OTHER:
            return {delimiter: true, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_NUMERIC:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_SYMBOL:
            return {delimiter: true, prefix: '????????????'};
        }
        break;
      case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER:
      case JaPhoneticData.CharacterSet.FULL_WIDTH_NUMERIC:
      case JaPhoneticData.CharacterSet.FULL_WIDTH_SYMBOL:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_NUMERIC:
          case JaPhoneticData.CharacterSet.FULL_WIDTH_SYMBOL:
            return {delimiter: true, prefix: null};
        }
        break;
      case JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_LOWER:
        switch (lastCharacterSet) {
          case JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_UPPER:
            return {
              delimiter: true,
              prefix: JaPhoneticData.getDefaultPrefix(currentCharacterSet),
            };
        }
        return {delimiter: true, prefix: null};
      case JaPhoneticData.CharacterSet.OTHER:
        return {delimiter: true, prefix: null};
    }
    // Returns the default prefix.
    return {
      delimiter: true,
      prefix: JaPhoneticData.getDefaultPrefix(currentCharacterSet),
    };
  }

  /**
   * @param {JaPhoneticData.CharacterSet} characterSet
   * @return {boolean}
   */
  static alwaysReadPhonetically(characterSet) {
    switch (characterSet) {
      case JaPhoneticData.CharacterSet.HALF_WIDTH_SYMBOL:
      case JaPhoneticData.CharacterSet.FULL_WIDTH_SYMBOL:
      case JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_UPPER:
      case JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_LOWER:
      case JaPhoneticData.CharacterSet.OTHER:
        return true;
    }
    return false;
  }
};

/** @enum {number} */
JaPhoneticData.CharacterSet = {
  NONE: 0,
  HIRAGANA: 1,                             // '???'
  KATAKANA: 2,                             // '???'
  HIRAGANA_SMALL_LETTER: 3,                // '???'
  KATAKANA_SMALL_LETTER: 4,                // '???'
  HALF_WIDTH_KATAKANA: 5,                  // '???'
  HALF_WIDTH_KATAKANA_SMALL_LETTER: 6,     // '???'
  HALF_WIDTH_ALPHABET_UPPER: 7,            // 'A'
  HALF_WIDTH_ALPHABET_LOWER: 8,            // 'a'
  HALF_WIDTH_NUMERIC: 9,                   // '1'
  HALF_WIDTH_SYMBOL: 10,                   // '@'
  FULL_WIDTH_ALPHABET_UPPER: 11,           // '???'
  FULL_WIDTH_ALPHABET_LOWER: 12,           // '???'
  FULL_WIDTH_NUMERIC: 13,                  // '???'
  FULL_WIDTH_SYMBOL: 14,                   // '???'
  FULL_WIDTH_CYRILLIC_OR_GREEK_UPPER: 15,  // '??'
  FULL_WIDTH_CYRILLIC_OR_GREEK_LOWER: 16,  // '??'
  OTHER: 17,                               // Kanji and unsupported symbols
};

/**
 *  @type {Map<JaPhoneticData.CharacterSet, string>}
 *  @const
 */
JaPhoneticData.DEFAULT_PREFIX = new Map([
  // '???'
  [JaPhoneticData.CharacterSet.HIRAGANA, '????????????'],
  // '???'
  [JaPhoneticData.CharacterSet.KATAKANA, '????????????'],
  // '???'
  [JaPhoneticData.CharacterSet.HIRAGANA_SMALL_LETTER, '???????????? ????????????'],
  // '???'
  [JaPhoneticData.CharacterSet.KATAKANA_SMALL_LETTER, '???????????? ????????????'],
  // '???'
  [JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA, '????????????'],
  // '???'
  [
    JaPhoneticData.CharacterSet.HALF_WIDTH_KATAKANA_SMALL_LETTER,
    '???????????? ????????????',
  ],
  // 'A'
  [JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_UPPER, '????????????'],
  // 'a'
  [JaPhoneticData.CharacterSet.HALF_WIDTH_ALPHABET_LOWER, '????????????'],
  // '1'
  [JaPhoneticData.CharacterSet.HALF_WIDTH_NUMERIC, '????????????'],
  // '@'
  [JaPhoneticData.CharacterSet.HALF_WIDTH_SYMBOL, '????????????'],
  // '???'
  [JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_UPPER, '????????????????????????'],
  // '???'
  [JaPhoneticData.CharacterSet.FULL_WIDTH_ALPHABET_LOWER, '????????????'],
  // '???'
  [JaPhoneticData.CharacterSet.FULL_WIDTH_NUMERIC, '????????????'],
  // '???'
  [JaPhoneticData.CharacterSet.FULL_WIDTH_SYMBOL, '????????????'],
  // '??'
  [JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_UPPER, '????????????'],
  // '??'
  [JaPhoneticData.CharacterSet.FULL_WIDTH_CYRILLIC_OR_GREEK_LOWER, '?????????'],
]);

/**
 * This object maps small letters of Kana to their large equivalents.
 * @type {Map<string, string>}
 * @const
 */
JaPhoneticData.SMALL_TO_LARGE = new Map([
  // Hiragana
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  // Katakana
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  // HalfWidthKatakana
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
  ['???', '???'],
]);
