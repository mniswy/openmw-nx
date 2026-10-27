#!/usr/bin/env python3
"""Make OpenMW's book / dialogue text wrap inside Chinese text.

bookpage.cpp splits text into "words" separated by breaking spaces and only
breaks a line between words. Chinese has no spaces, so a whole paragraph is a
single word and it runs off the right edge of the page.

This patch treats every CJK character as a word of its own, so the existing
wrap logic in add_partial_text() can break the line after any character.
"""
import io
import os

ROOT = os.getcwd()
CPP = os.path.join(ROOT, "apps", "openmw", "mwgui", "bookpage.cpp")

HELPER = r"""
    // CJK: characters that may start/end a line anywhere
    bool ucsCJK(Utf8Stream::UnicodeChar codePoint)
    {
        return (codePoint >= 0x2E80 && codePoint <= 0x2FFF)    // radicals
            || (codePoint >= 0x3000 && codePoint <= 0x303F)    // CJK punctuation
            || (codePoint >= 0x3040 && codePoint <= 0x30FF)    // kana
            || (codePoint >= 0x3400 && codePoint <= 0x4DBF)    // ext A
            || (codePoint >= 0x4E00 && codePoint <= 0x9FFF)    // unified ideographs
            || (codePoint >= 0xF900 && codePoint <= 0xFAFF)    // compatibility
            || (codePoint >= 0xFF00 && codePoint <= 0xFFEF);   // fullwidth forms
    }
"""

OLD_WORD_LOOP = """                while (!stream.eof() && !ucsLineBreak(stream.peek()) && !ucsBreakingSpace(stream.peek()))
                {
                    std::optional<MyGUI::GlyphInfo> info = getGlyphInfo(style->mFont, stream.peek());
                    if (info)
                        wordWidth += static_cast<int>(info->advance + info->bearingX);
                    stream.consume();
                }

                Utf8Stream::Point extent = stream.current();

                if (lead == extent)
                    break;

                if (lead != origin)
                    mPartialWhitespace.emplace_back(style, lead, origin, spaceWidth);
                if (origin != extent)
                    mPartialWord.emplace_back(style, origin, extent, wordWidth);"""

NEW_WORD_LOOP = """                Utf8Stream::UnicodeChar lastChar = 0;

                while (!stream.eof() && !ucsLineBreak(stream.peek()) && !ucsBreakingSpace(stream.peek()))
                {
                    lastChar = stream.peek();
                    std::optional<MyGUI::GlyphInfo> info = getGlyphInfo(style->mFont, lastChar);
                    if (info)
                        wordWidth += static_cast<int>(info->advance + info->bearingX);
                    stream.consume();

                    // a CJK character ends the current word: lines may break after it
                    if (ucsCJK(lastChar))
                        break;
                }

                Utf8Stream::Point extent = stream.current();

                if (lead == extent)
                    break;

                if (lead != origin)
                    mPartialWhitespace.emplace_back(style, lead, origin, spaceWidth);
                if (origin != extent)
                    mPartialWord.emplace_back(style, origin, extent, wordWidth);

                // flush early so the wrapper can put the line break between
                // two CJK characters instead of past the page edge
                if (ucsCJK(lastChar))
                    add_partial_text();"""


def read(p):
    with io.open(p, "r", encoding="utf-8", newline="") as f:
        return f.read()


def write(p, t):
    with io.open(p, "w", encoding="utf-8", newline="") as f:
        f.write(t)


def main():
    if not os.path.exists(CPP):
        raise SystemExit("not found: %s (run from the repo root)" % CPP)

    t = read(CPP)

    if "bool ucsCJK(" in t:
        print("already patched")
        return

    # 1) helper next to the other ucs* predicates (end of the anonymous namespace)
    anchor = """            case 0x3000: // IDEOGRAPHIC SPACE
                return true;
            default:
                return false;
        }
    }
}"""
    if t.count(anchor) != 1:
        raise SystemExit("ucsBreakingSpace anchor not found (%d)" % t.count(anchor))
    t = t.replace(anchor, anchor[:-2] + HELPER + "}", 1)

    # 2) the word scanner
    if t.count(OLD_WORD_LOOP) != 1:
        raise SystemExit("word loop anchor not found (%d)" % t.count(OLD_WORD_LOOP))
    t = t.replace(OLD_WORD_LOOP, NEW_WORD_LOOP, 1)

    write(CPP, t)
    print("patched:", CPP)


if __name__ == "__main__":
    main()
