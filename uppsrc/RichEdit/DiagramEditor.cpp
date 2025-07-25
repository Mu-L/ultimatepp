#include "RichEdit.h"

namespace Upp {

#define IMAGECLASS DiagramImg
#define IMAGEFILE <RichEdit/Diagram.iml>
#include <Draw/iml_source.h>

DiagramEditor::DiagramEditor()
{
	AddFrame(toolbar);

	Add(text_editor);
	text_editor.NoRuler().ViewBorder(0);
	text_editor.WhenRefreshBar = [=] { SetBar(); };
	text_editor.WhenEnter << [=] { FinishText(); };
	text_editor.WhenEsc << [=] { edit_text = false; Sync(); };
	text_editor.WhenAction << [=] {
		SyncEditorRect();
	};

	ink.ColorImage(DiagramImg::Ink())
	   .NullImage(DiagramImg::InkNull())
	   .StaticImage(DiagramImg::InkA());
	ink.Tip(t_("Line color"));
	ink << [=] { SetAttrs(ATTR_INK); };

	paper.ColorImage(DiagramImg::Paper())
	   .NullImage(DiagramImg::PaperNull())
	   .StaticImage(DiagramImg::PaperA());
	paper.Tip(t_("Background color"));

	paper << [=] { SetAttrs(ATTR_PAPER); };
	shape << [=] { SetAttrs(ATTR_SHAPE); };

	line_start << [=] { SetAttrs(ATTR_CAP0); };
	line_end << [=] { SetAttrs(ATTR_CAP1); };

	for(int i = 0; i < 10; i++)
		line_width.Add(i);
	line_width << [=] { SetAttrs(ATTR_WIDTH); };

	line_dash << [=] { SetAttrs(ATTR_DASH); };
	
	tl[0].shape = DiagramItem::SHAPE_LINE;
	tl[1].shape = DiagramItem::SHAPE_ROUNDRECT;

	ResetUndo();
	Sync();

	sb.AutoHide();
	sb.WithSizeGrip();
	AddFrame(sb);
	sb.WhenScroll << [=] { Sync(); };

	editor = true;
	
	GetAttrs(DiagramItem());
}

void DiagramEditor::SetupDark(ColorPusher& c) const
{
	c.AllowDarkContent(allow_dark_content);
	c.DarkContent(dark_content);
}

bool DiagramEditor::IsDarkContent() const
{
	return dark_content || allow_dark_content && IsDarkTheme();
}

DiagramEditor& DiagramEditor::DarkContent(bool b)
{
	dark_content = b;
	Sync();
	return *this;
}

DiagramEditor& DiagramEditor::AllowDarkContent(bool b)
{
	allow_dark_content = b;
	Sync();
	return *this;
}

void DiagramEditor::Skin()
{
	SetBar();

	int cy = GetStdFontCy();

	Size shape_sz = Size(DPI(24), cy);

	shape.ClearList();
	for(int i = 0; i < DiagramItem::SHAPE_COUNT; i++) {
		DiagramItem m;
		m.pt[0] = Point(2, 2);
		m.pt[1] = Point(shape_sz.cx - 2, cy - 2);
		m.width = DPI(1);
		m.shape = i;
		shape.Add(i, MakeIcon(m, shape_sz));
	}

	struct Dialine : DiagramItem {
		Dialine() {
			shape = SHAPE_LINE;
			pt[0].y = pt[1].y = 7;
			pt[0].x = -9999;
			pt[1].x = 9999;
		}
	};

	auto LDL = [=](DropList& dl, bool left) {
		dl.ClearList();
		for(int i = DiagramItem::CAP_NONE; i < DiagramItem::CAP_COUNT; i++) {
			dl.Add(i, CapIcon(left ? i : 0, left ? 0 : i));
		}
	};

	LDL(line_start, true);
	LDL(line_end, false);

	line_dash.ClearList();
	for(int i = 0; i < DiagramItem::DASH_COUNT; i++) {
		Dialine m;
		m.dash = i;
		line_dash.Add(i, MakeIcon(m, shape_sz));
	}
}

Image DiagramEditor::MakeIcon(DiagramItem& m, Size isz)
{
	struct IconMaker : ImageMaker {
		Size         isz;
		DiagramItem  m;
		bool         dark;
		String Key() const override {
			String key = StoreAsString(const_cast<DiagramItem&>(m));
			RawCat(key, isz);
			RawCat(key, dark);
			return key;
		}
		Image Make() const override {
			ImagePainter iw(isz);
			iw.Clear();
			m.Paint(iw, dark ? DiagramItem::DARK : 0);
			return iw;
		}
	};

	IconMaker mk;
	mk.m = m;
	mk.isz = isz;
	mk.dark = IsDarkContent();
	return MakeImage(mk);
}

Image DiagramEditor::ShapeIcon(int i)
{
	Size isz = IconSz();
	DiagramItem m;
	m.pt[0] = Point(2, 2);
	m.pt[1] = Point(isz.cx - 2, isz.cy - 2);
	m.width = DPI(1);
	m.shape = i;
	m.paper = Null;
	return MakeIcon(m, isz);
}

Image DiagramEditor::CapIcon(int start, int end)
{
	Size isz = IconSz();
	DiagramItem m;
	m.pt[0] = Point(DPI(6), isz.cy / 2);
	m.pt[1] = Point(isz.cx - DPI(6), isz.cy / 2);
	m.shape = DiagramItem::SHAPE_LINE;
	m.width = DPI(2);
	m.cap[0] = start;
	m.cap[1] = end;
	return MakeIcon(m, isz);
}

Image DiagramEditor::DashIcon(int i)
{
	Size isz = IconSz();
	DiagramItem m;
	m.pt[0] = Point(DPI(6), isz.cy / 2);
	m.pt[1] = Point(isz.cx - DPI(6), isz.cy / 2);
	m.shape = DiagramItem::SHAPE_LINE;
	m.width = DPI(2);
	m.dash = i;
	return MakeIcon(m, isz);
}

void DiagramEditor::Paint(Draw& w)
{
	RTIMING("Paint");
	DrawPainter iw(w, GetSize());
	iw.Co();
	iw.Clear(SWhite());

	iw.Scale(GetZoom());
	iw.Offset(-(Point)sb);
	Size dsz = data.GetSize();
	iw.Move(dsz.cx, 0).Line(dsz.cx, dsz.cy).Line(0, dsz.cy).Stroke(0.2, SColorHighlight());

	if(data.item.GetCount() == 0) {
		iw.DrawText(DPI(30), DPI(30), "Right-click to insert item(s)", ArialZ(10).Italic(), SLtGray());
		iw.DrawText(DPI(30), DPI(50), "Double-click to edit text", ArialZ(10).Italic(), SLtGray());
	}
	
	if(display_grid)
		for(int x = 0; x < dsz.cx; x += 8)
			for(int y = 0; y < dsz.cy; y += 8) {
				if(((x | y) & 15) == 0) {
					iw.DrawRect(x - 2, y, 5, 1, Blend(SWhite(), SGreen(), 60));
					iw.DrawRect(x, y - 2, 1, 5, Blend(SWhite(), SGreen(), 60));
				}
				else
					iw.DrawRect(x, y, 1, 1, Blend(SWhite(), SGreen()));
			}

	dark = IsDarkContent();
	data.Paint(iw, *this);

	if(HasCapture() && doselection) {
		Rect r(dragstart, dragcurrent);
		r.Normalize();
		iw.Rectangle(r).Fill(30 * SColorHighlight()).Stroke(1, LtRed()).Dash("4").Stroke(1, White());
	}
}

void DiagramEditor::Sync()
{
	Refresh();
	SetBar();
	sb.SetTotal(data.GetSize() + Point(10, 10));
	sb.SetPage(GetSize() / GetZoom());
	sb.SetLine(8, 8);
	SyncEditor();
}

void DiagramEditor::Layout()
{
	Sync();
}

void DiagramEditor::ResetUndo()
{
	undoredo.Reset(GetCurrent());
}

void DiagramEditor::Commit()
{
	if(IsCursor()) {
		DiagramItem& m = CursorItem();
		if(!m.IsLine())
			m.Normalize();
	}
	if(undoredo.Commit(GetCurrent())) {
		SetBar();
		Sync();
	}
	conns.Clear();
}

String DiagramEditor::GetCurrent()
{
	return StoreAsString(data);
}

bool DiagramEditor::SetCurrent(const String& s)
{
	KillCursor();
//	DLOG("===========");
//	DDUMP(data.item.GetCount());
	bool b = LoadFromString(data, s);
//	DDUMP(data.item.GetCount());
//	DDUMP(cursor);
	Sync();
	return b;
}

void DiagramEditor::CancelSelection()
{
	FinishText();
	sel.Clear();
	KillCursor();
	Sync();
}

void DiagramEditor::Zoom()
{
	zoom_percent = (zoom_percent / 25 + 1) * 25;
	if(zoom_percent > 400)
		zoom_percent = 25;
	Sync();
}

void DiagramEditor::MouseWheel(Point, int zdelta, dword keyflags) {
	if(keyflags & K_CTRL) {
		zoom_percent = clamp((zoom_percent / 25 + sgn(zdelta)) * 25, 25, 400);
		Sync();
		return;
	}
	if(keyflags & K_SHIFT)
		sb.WheelX(zdelta);
	else
		sb.WheelY(zdelta);
}

void DiagramEditor::HorzMouseWheel(Point, int zdelta, dword)
{
	sb.WheelX(zdelta);
}

String DiagramEditor::Save() const
{
	StringBuffer r;
	data.Save(r);
	return String(r);
}

bool DiagramEditor::Load(const String& s)
{
	CParser p(s);
	try {
		data.Load(p);
	}
	catch(CParser::Error) {
		Reset();
		return false;
	}
	Reset();
	return true;
}

void DiagramEditor::Reset()
{
	doselection = false;
	grid = true;
	edit_text = false;
	ResetUndo();
	Sync();
}

bool DiagramEditor::Key(dword key, int count)
{
	switch(key) {
	case K_ESCAPE:
		if(tool >= 0) {
			tool = -1;
			SetBar();
			return true;
		}
		if(IsCursor()) {
			KillCursor();
			return true;
		}
		break;
	}
	return Ctrl::Key(key, count);
}

}