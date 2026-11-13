"""Build the illustrated Chinese user guide from its Markdown source."""
from pathlib import Path
import argparse
import html
from urllib.parse import quote

import pymupdf as fitz
from markdown_it import MarkdownIt
from PIL import Image as PILImage
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas
from reportlab.platypus import (
    BaseDocTemplate, Flowable, Frame, Image, KeepTogether, PageBreak,
    PageTemplate, Paragraph, Spacer, Table, TableStyle,
)
from reportlab.platypus.tableofcontents import TableOfContents

ROOT = Path(__file__).resolve().parents[1]
WIDTH, HEIGHT = A4
MARGIN = 44
CONTENT = WIDTH - MARGIN * 2
INK = colors.HexColor("#26364B")
ACCENT = colors.HexColor("#426A83")
MUTED = colors.HexColor("#65758A")
PALE = colors.HexColor("#EEF3F6")
PINK = colors.HexColor("#F6E6E9")
GREEN = colors.HexColor("#E6F0E8")
SOURCE_URL = "https://github.com/asidjwiaijd/AMac/blob/main/"


def fonts(regular, bold):
    pdfmetrics.registerFont(TTFont("GuideCN", str(regular)))
    pdfmetrics.registerFont(TTFont("GuideBold", str(bold)))
    pdfmetrics.registerFontFamily("GuideCN", normal="GuideCN", bold="GuideBold", italic="GuideCN", boldItalic="GuideBold")


def rounded(c, x, y, w, h, fill=PALE, stroke=None):
    c.setFillColor(fill)
    c.setStrokeColor(stroke or fill)
    c.roundRect(x, y, w, h, 9, fill=1, stroke=bool(stroke))


def text(c, x, y, content, size=10, bold=False, color=INK):
    c.setFillColor(color)
    c.setFont("GuideBold" if bold else "GuideCN", size)
    c.drawString(x, y, content)


def arrow(c, points, color=ACCENT):
    c.setStrokeColor(color)
    c.setLineWidth(1.5)
    p = c.beginPath()
    p.moveTo(*points[0])
    for point in points[1:]:
        p.lineTo(*point)
    c.drawPath(p)
    (x0, y0), (x, y) = points[-2:]
    if abs(x-x0) > abs(y-y0):
        sign = 1 if x > x0 else -1
        c.line(x, y, x-sign*6, y+3)
        c.line(x, y, x-sign*6, y-3)
    else:
        sign = 1 if y > y0 else -1
        c.line(x, y, x+3, y-sign*6)
        c.line(x, y, x-3, y-sign*6)


def diagram(name, size, draw):
    temporary = ROOT / "tmp/pdfs" / (name + ".pdf")
    c = canvas.Canvas(str(temporary), pagesize=size)
    c.setFillColor(colors.white)
    c.rect(0, 0, *size, fill=1, stroke=0)
    draw(c)
    c.save()
    with fitz.open(temporary) as document:
        document[0].get_pixmap(matrix=fitz.Matrix(3, 3), alpha=False).save(ROOT / "guide/images" / (name + ".png"))


def diagrams():
    def connections(c):
        rounded(c, 195, 55, 125, 170, colors.HexColor("#F4F2EC"), colors.HexColor("#B8BFCA"))
        rounded(c, 206, 122, 103, 85, colors.white, colors.HexColor("#B8BFCA"))
        text(c, 229, 171, "aMac", 19, True)
        text(c, 216, 145, "屏幕上的 Mac", 10)
        text(c, 218, 96, "电池保持连接", 10, True)
        text(c, 222, 76, "microSD 留在机内", 9)
        rounded(c, 12, 151, 149, 65, GREEN)
        text(c, 24, 189, "充电器 / 外部电源", 12, True)
        text(c, 24, 167, "USB1：供电与充电", 10)
        arrow(c, [(161, 183), (195, 183)])
        rounded(c, 352, 151, 145, 65, PALE)
        text(c, 364, 189, "Windows 电脑", 12, True)
        text(c, 364, 167, "USB2：数据与控制", 10)
        arrow(c, [(352, 182), (320, 182)])
        rounded(c, 12, 57, 149, 65, PINK)
        text(c, 24, 95, "USB 鼠标 / 键盘", 12, True)
        text(c, 24, 73, "插到设备的键鼠接口", 10)
        arrow(c, [(161, 88), (195, 88)])
        rounded(c, 352, 57, 145, 65, colors.HexColor("#F6F4ED"))
        text(c, 364, 95, "USB2 不为整机供电", 11, True)
        text(c, 364, 73, "仍需电池或 USB1", 10)
        text(c, 12, 22, "功能连接示意，请按实物上的接口标记识别位置。", 9, color=MUTED)
    diagram("connections", (510, 250), connections)

    def storage(c):
        rounded(c, 6, 33, 498, 187, colors.HexColor("#F5F6F8"), colors.HexColor("#C7CFD8"))
        text(c, 22, 196, "设备内的 microSD 卡", 14, True)
        rounded(c, 20, 123, 179, 49, colors.white)
        text(c, 31, 151, "Q650.ROM", 11, True)
        text(c, 31, 134, "启动所需的 ROM", 9)
        rounded(c, 20, 59, 179, 49, colors.white)
        text(c, 31, 87, "basilisk_settings.txt", 10, True)
        text(c, 31, 70, "设备启动与网络配置", 9)
        rounded(c, 219, 54, 267, 120, PALE)
        text(c, 234, 149, "Macintosh.dsk", 14, True)
        text(c, 234, 128, "Mac 里面的一整块硬盘", 10)
        for x, y, label in [(234, 89, "系统与应用"), (358, 89, "游戏与进度"), (234, 61, "你的文档"), (358, 61, "壁纸与彩蛋")]:
            rounded(c, x, y, 112, 23, colors.white)
            text(c, x+8, y+7, label, 9)
        text(c, 22, 11, "在 Mac 里保存文件，最终写入 SD 上的虚拟硬盘。", 10, color=ACCENT)
    diagram("storage", (510, 236), storage)

    def power(c):
        for x, y, title, detail, fill in [
            (15, 164, "正常运行", "保存正在使用的内容", PALE),
            (287, 164, "正常关机", "主动关机 / 低电压保护", PINK),
            (287, 37, "深度休眠", "保留 RTC，每约 5 秒检查", colors.HexColor("#F1EEF7")),
            (15, 37, "重新开始充电", "检测确认后重新启动 Mac", GREEN),
        ]:
            rounded(c, x, y, 208, 61, fill)
            text(c, x+15, y+35, title, 15, True)
            text(c, x+15, y+15, detail, 10)
        arrow(c, [(223, 194), (287, 194)])
        arrow(c, [(390, 164), (390, 98)])
        text(c, 400, 125, "清理完毕", 9, color=MUTED)
        arrow(c, [(287, 67), (223, 67)])
        arrow(c, [(119, 98), (119, 164)])
        text(c, 20, 13, "键鼠不能唤醒深睡；满电插线未开始充电时可能不触发。", 9, color=MUTED)
    diagram("power-flow", (510, 244), power)


class Cover(Flowable):
    def __init__(self):
        super().__init__()
        self.width, self.height = CONTENT, 706
    def draw(self):
        c = self.canv
        rounded(c, 0, 3, self.width, 700, colors.HexColor("#FBF8F1"))
        rounded(c, 24, 604, 5, 66, colors.HexColor("#D996A4"))
        text(c, 43, 634, "aMac", 46, True)
        text(c, 43, 588, "新手使用指南", 29, True)
        text(c, 43, 560, "从第一次开机，到日常使用与不拆机维护", 12, color=MUTED)
        image = ROOT / "guide/images/desktop.png"
        rounded(c, 34, 196, 440, 340, colors.white, colors.HexColor("#B7BDC7"))
        c.drawImage(str(image), 44, 208, width=420, height=315)
        text(c, 43, 163, "图文详解 / 电脑键鼠控制 / 保留彩蛋惊喜", 12, True)
        text(c, 43, 130, "设备固件 0.6.3  ·  Windows 控制台 0.1.0", 10, color=MUTED)
        text(c, 43, 109, "2026 年 11 月 13 日", 10, color=MUTED)
        text(c, 43, 53, "给第一次使用这台小电脑的你", 14)


class GuideDoc(BaseDocTemplate):
    def __init__(self, filename):
        super().__init__(str(filename), pagesize=A4, leftMargin=MARGIN, rightMargin=MARGIN,
                         topMargin=48, bottomMargin=45, title="aMac 新手使用指南",
                         author="aMac 项目", subject="固件 0.6.3 与 Windows 键鼠控制台 0.1.0")
        frame = Frame(MARGIN, 45, CONTENT, HEIGHT-93, leftPadding=0, rightPadding=0,
                      topPadding=0, bottomPadding=0)
        self.addPageTemplates(PageTemplate(id="Guide", frames=[frame], onPage=self.decorate))
    def decorate(self, c, _doc):
        if self.page > 1:
            text(c, MARGIN, HEIGHT-29, "aMac / 新手使用指南", 8.5, color=MUTED)
            c.setStrokeColor(colors.HexColor("#DCE2E8"))
            c.setLineWidth(0.5)
            c.line(MARGIN, HEIGHT-36, WIDTH-MARGIN, HEIGHT-36)
        text(c, MARGIN, 23, "固件 0.6.3 / 控制台 0.1.0", 8, color=MUTED)
        c.setFillColor(MUTED)
        c.setFont("GuideCN", 8)
        c.drawRightString(WIDTH-MARGIN, 23, f"{self.page:02d}")
    def afterFlowable(self, flowable):
        if hasattr(flowable, "toc_key"):
            self.canv.bookmarkPage(flowable.toc_key)
            title = flowable.getPlainText()
            self.canv.addOutlineEntry(title, flowable.toc_key, 0)
            self.notify("TOCEntry", (0, title, self.page, flowable.toc_key))


def make_styles():
    base = dict(fontName="GuideCN", fontSize=10.2, leading=16.5, textColor=INK, wordWrap="CJK", spaceAfter=6)
    body = ParagraphStyle("Body", **base)
    return {
        "body": body,
        "h2": ParagraphStyle("H2", parent=body, fontName="GuideBold", fontSize=20, leading=29, spaceAfter=16, keepWithNext=True),
        "h3": ParagraphStyle("H3", parent=body, fontName="GuideBold", fontSize=12.2, leading=20, spaceBefore=8, spaceAfter=5, keepWithNext=True),
        "caption": ParagraphStyle("Caption", parent=body, fontSize=8.5, leading=13, textColor=MUTED, spaceBefore=5, spaceAfter=13),
        "list": ParagraphStyle("List", parent=body, leftIndent=18, bulletIndent=0, bulletFontName="GuideCN", bulletFontSize=10.2, spaceAfter=6),
        "table": ParagraphStyle("Table", parent=body, fontSize=9.0, leading=14, spaceAfter=0),
        "th": ParagraphStyle("TableHeader", parent=body, fontName="GuideBold", fontSize=9.3, leading=14, textColor=colors.white, spaceAfter=0),
        "note": ParagraphStyle("Note", parent=body, fontSize=10, leading=17, spaceAfter=0),
    }


def inline(children):
    result = []
    for token in children or []:
        if token.type == "text":
            result.append(html.escape(token.content))
        elif token.type == "code_inline":
            result.append('<font color="#426A83">' + html.escape(token.content) + '</font>')
        elif token.type in ("softbreak", "hardbreak"):
            result.append(" " if token.type == "softbreak" else "<br/>")
        elif token.type == "strong_open": result.append("<b>")
        elif token.type == "strong_close": result.append("</b>")
        elif token.type == "em_open": result.append("<i>")
        elif token.type == "em_close": result.append("</i>")
        elif token.type == "link_open":
            url = token.attrGet("href")
            if not url.startswith(("http:", "https:")):
                url = SOURCE_URL + quote(url, safe="/#")
            result.append('<link href="' + html.escape(url, quote=True) + '" color="#426A83">')
        elif token.type == "link_close": result.append("</link>")
    return "".join(result)


def figure(src):
    path = ROOT / src
    with PILImage.open(path) as im:
        w, h = im.size
    target = CONTENT if path.stem in ("connections", "storage", "power-flow") else 380
    if path.stem == "startup-settings": target = 330
    if path.stem == "storage": target = 460
    if path.stem == "desktop-controller": target = 465
    return Image(str(path), width=target, height=target*h/w, hAlign="CENTER")


def render_markdown(source, styles):
    tokens = MarkdownIt("commonmark").enable("table").parse(source)
    story = []
    lists = []
    paragraph_bullet = None
    quote_depth = 0
    i = 0
    while i < len(tokens):
        token = tokens[i]
        if token.type == "heading_open":
            level = int(token.tag[1])
            content = tokens[i+1]
            if level == 2:
                story.append(PageBreak())
                p = Paragraph(inline(content.children), styles["h2"])
                p.toc_key = f"chapter-{len([f for f in story if hasattr(f, 'toc_key')])+1}"
                story.append(p)
            elif level == 3:
                story.append(Paragraph(inline(content.children), styles["h3"]))
            i += 3
            continue
        if token.type in ("bullet_list_open", "ordered_list_open"):
            lists.append([token.type == "ordered_list_open", int(token.attrGet("start") or 1), len(story)])
        elif token.type in ("bullet_list_close", "ordered_list_close"):
            numbered, _n, start = lists.pop()
            if numbered and len(story)-start <= 12:
                story[start:] = [KeepTogether(story[start:])]
        elif token.type == "list_item_open":
            numbered, n, _start = lists[-1]
            paragraph_bullet = f"{n}." if numbered else "•"
            lists[-1][1] += 1
        elif token.type == "list_item_close": paragraph_bullet = None
        elif token.type == "blockquote_open": quote_depth += 1
        elif token.type == "blockquote_close": quote_depth -= 1
        elif token.type == "table_open":
            rows, current, header = [], [], False
            i += 1
            while tokens[i].type != "table_close":
                t = tokens[i]
                if t.type == "tr_open": current = []
                elif t.type in ("th_open", "td_open"):
                    header = t.type == "th_open"
                elif t.type == "inline":
                    current.append(Paragraph(inline(t.children), styles["th" if header else "table"]))
                elif t.type == "tr_close": rows.append(current)
                i += 1
            count = len(rows[0])
            ratios = [0.31, 0.69] if count == 2 else [0.23, 0.34, 0.43]
            table = Table(rows, colWidths=[CONTENT*r for r in ratios], repeatRows=1, hAlign="LEFT")
            table.setStyle(TableStyle([
                ("BACKGROUND", (0, 0), (-1, 0), ACCENT),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.HexColor("#F2F5F7"), colors.white]),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), 9), ("RIGHTPADDING", (0, 0), (-1, -1), 9),
                ("TOPPADDING", (0, 0), (-1, -1), 6), ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
                ("LINEBELOW", (0, 0), (-1, 0), 0.5, ACCENT),
            ]))
            story.extend([table, Spacer(1, 12)])
        elif token.type == "paragraph_open":
            content = tokens[i+1]
            children = content.children or []
            images = [child for child in children if child.type == "image"]
            # Title/intro are represented on the designed cover and contents.
            if not any(hasattr(f, "toc_key") for f in story):
                i += 3
                continue
            if images:
                flowables = [figure(images[0].attrGet("src"))]
                next_index = i+3
                if next_index+1 < len(tokens) and tokens[next_index].type == "paragraph_open" and tokens[next_index+1].content.startswith("图 "):
                    flowables.append(Paragraph(inline(tokens[next_index+1].children), styles["caption"]))
                    i += 3
                story.append(KeepTogether(flowables))
            else:
                value = inline(children)
                if quote_depth:
                    table = Table([[Paragraph(value, styles["note"])]], colWidths=[CONTENT])
                    table.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), PALE),
                        ("BOX", (0, 0), (-1, -1), 0.5, colors.HexColor("#CEDAE3")),
                        ("LEFTPADDING", (0, 0), (-1, -1), 12), ("RIGHTPADDING", (0, 0), (-1, -1), 12),
                        ("TOPPADDING", (0, 0), (-1, -1), 10), ("BOTTOMPADDING", (0, 0), (-1, -1), 10)]))
                    story.extend([table, Spacer(1, 10)])
                elif paragraph_bullet:
                    story.append(Paragraph(value, styles["list"], bulletText=paragraph_bullet))
                    paragraph_bullet = None
                else:
                    story.append(Paragraph(value, styles["body"]))
            i += 3
            continue
        i += 1
    return story


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--font", type=Path, default=Path("C:/Windows/Fonts/Deng.ttf"))
    parser.add_argument("--bold-font", type=Path, default=Path("C:/Windows/Fonts/Dengb.ttf"))
    args = parser.parse_args()
    for path in (ROOT/"tmp/pdfs", ROOT/"output/pdf", ROOT/"guide/images"):
        path.mkdir(parents=True, exist_ok=True)
    fonts(args.font, args.bold_font)
    diagrams()
    styles = make_styles()
    toc = TableOfContents()
    toc.levelStyles = [ParagraphStyle("Contents", fontName="GuideCN", fontSize=11,
        leading=26, textColor=INK, leftIndent=0, firstLineIndent=0, rightIndent=25)]
    story = [Cover(), PageBreak(), Paragraph("先找到你需要的那一页", styles["h2"]),
             Paragraph("只想先用起来，可以从第 01 节开始；需要连接电脑时，再读第 08、14、15 节。", styles["body"]),
             Spacer(1, 12), toc]
    story += render_markdown((ROOT/"AMAC_USER_GUIDE.md").read_text(encoding="utf-8"), styles)
    output = ROOT/"output/pdf/aMac_User_Guide.pdf"
    GuideDoc(output).multiBuild(story)
    with fitz.open(output) as document:
        text_all = "\n".join(page.get_text() for page in document)
        assert "Ctrl + Alt + Esc" in text_all
        assert "4.10 V" in text_all
        assert len(document.get_toc()) == 18
        print(f"PDF: {output}\nPages: {len(document)}\nExtracted characters: {len(text_all)}")


if __name__ == "__main__":
    main()
