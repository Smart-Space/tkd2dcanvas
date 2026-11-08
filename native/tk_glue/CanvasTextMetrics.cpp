#include "CanvasTextMetrics.h"

#include <algorithm>
#include <cmath>

namespace tkd2dcanvas {

namespace {

std::size_t Utf8CharLength(const char* p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;  // stray continuation byte: advance one byte
}

class TextMeasurer {
public:
    TextMeasurer(Tcl_Interp* interp, Tk_Window tkwin, const TkFontInfo& font)
        : interp_(interp), tkwin_(tkwin), font_(font) {}

    double Measure(const std::string& line) {
        if (line.empty()) return 0.0;
        int width = 0;
        if (MeasureTkFont(interp_, tkwin_, font_, line, width)) {
            return static_cast<double>(width);
        }
        Tcl_ResetResult(interp_);
        return static_cast<double>(line.size()) * font_.size * 0.6;
    }

private:
    Tcl_Interp* interp_;
    Tk_Window tkwin_;
    const TkFontInfo& font_;
};

void FinalizeMaxWidth(MeasuredTextLayout& result) {
    result.maxLineWidth = 0.0;
    for (const double w : result.lineWidths) {
        result.maxLineWidth = std::max(result.maxLineWidth, w);
    }
}

template <typename ParagraphFn>
MeasuredTextLayout SplitParagraphs(const std::string& normalized, ParagraphFn&& layoutParagraph) {
    MeasuredTextLayout result;
    const char* cursor = normalized.c_str();
    const char* end = cursor + normalized.size();
    while (true) {
        const char* nl = cursor;
        while (nl < end && *nl != '\n') ++nl;
        layoutParagraph(std::string(cursor, nl), result);
        if (nl == end) break;
        cursor = nl + 1;
    }
    if (result.lines.empty()) {
        result.lines.push_back("");
        result.lineWidths.push_back(0.0);
    }
    FinalizeMaxWidth(result);
    return result;
}

void WrapChars(TextMeasurer& measurer, const std::string& word, double wrapWidth, MeasuredTextLayout& result);

MeasuredTextLayout WrapWords(TextMeasurer& measurer, const std::string& normalized, double wrapWidth) {
    const double spaceWidth = measurer.Measure(" ");
    return SplitParagraphs(normalized, [&](const std::string& paragraph, MeasuredTextLayout& result) {
        std::string current;
        double currentWidth = 0.0;
        auto pushCurrent = [&]() {
            result.lines.push_back(current);
            result.lineWidths.push_back(currentWidth);
            current.clear();
            currentWidth = 0.0;
        };

        const char* p = paragraph.c_str();
        while (*p) {
            const char* wordBegin = p;
            while (*wordBegin == ' ' || *wordBegin == '\t') ++wordBegin;
            if (*wordBegin == '\0') break;
            const char* wordEnd = wordBegin;
            while (*wordEnd && *wordEnd != ' ' && *wordEnd != '\t') ++wordEnd;
            const std::string word(wordBegin, wordEnd);
            const double wordWidth = measurer.Measure(word);
            if (current.empty() && wordWidth > wrapWidth) {
                WrapChars(measurer, word, wrapWidth, result);
                current.clear();
                currentWidth = 0.0;
                p = wordEnd;
                continue;
            }
            if (current.empty()) {
                current = word;
                currentWidth = wordWidth;
            } else if (currentWidth + spaceWidth + wordWidth <= wrapWidth) {
                current += ' ';
                current += word;
                currentWidth += spaceWidth + wordWidth;
            } else {
                pushCurrent();
                current = word;
                currentWidth = wordWidth;
            }
            p = wordEnd;
        }
        pushCurrent();  // flush the paragraph's last line
    });
}

// Character-wise wrap of a single over-limit word (or any run).
void WrapChars(TextMeasurer& measurer, const std::string& word, double wrapWidth,
               MeasuredTextLayout& result) {
    std::string current;
    double currentWidth = 0.0;
    const char* p = word.c_str();
    const char* pEnd = p + word.size();
    while (p < pEnd) {
        const std::size_t chLen = Utf8CharLength(p);
        const std::string ch(p, p + chLen);
        const double chWidth = measurer.Measure(ch);
        if (!current.empty() && currentWidth + chWidth > wrapWidth) {
            result.lines.push_back(current);
            result.lineWidths.push_back(currentWidth);
            current.clear();
            currentWidth = 0.0;
        }
        current += ch;
        currentWidth += chWidth;
        p += chLen;
    }
    if (!current.empty()) {
        result.lines.push_back(current);
        result.lineWidths.push_back(currentWidth);
    }
}

}  // namespace

MeasuredTextLayout MeasureTextLayout(Tcl_Interp* interp, Tk_Window tkwin,
                                     const TkFontInfo& font, const std::string& text,
                                     double wrapWidth, const std::string& wrapMode) {
    TextMeasurer measurer(interp, tkwin, font);

    // Normalize CRLF/CR to LF so the paragraph splitter has one break kind.
    std::string normalized;
    normalized.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\r') {
            normalized += '\n';
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;  // CRLF
        } else {
            normalized += c;
        }
    }

    if (wrapWidth <= 0.0 || wrapMode == "none") {
        // No wrapping: every paragraph is one line, newlines preserved.
        return SplitParagraphs(normalized, [&](const std::string& paragraph,
                                               MeasuredTextLayout& result) {
            result.lines.push_back(paragraph);
            result.lineWidths.push_back(measurer.Measure(paragraph));
        });
    }

    if (wrapMode == "char") {
        return SplitParagraphs(normalized, [&](const std::string& paragraph,
                                               MeasuredTextLayout& result) {
            WrapChars(measurer, paragraph, wrapWidth, result);
        });
    }
    return WrapWords(measurer, normalized, wrapWidth);
}

}  // namespace tkd2dcanvas
