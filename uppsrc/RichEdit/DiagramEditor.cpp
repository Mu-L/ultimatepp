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
	ink << [=] { SetAttrs(); };

	paper.ColorImage(DiagramImg::Paper())
	   .NullImage(DiagramImg::PaperNull())
	   .StaticImage(DiagramImg::PaperA());
	paper.Tip(t_("Background color"));
	paper << [=] { SetAttrs(); };
	
	int cy = GetStdFontCy();
	
	auto MakeImage = [=](DiagramItem& m) -> Image {
		ImagePainter iw(DPI(24), cy);
		iw.Scale(DPI(1));
		iw.Clear();
		m.Paint(iw);
		return iw;
	};
	
	cy /= DPI(1);

	for(int i = 0; i < DiagramItem::SHAPE_COUNT; i++) {
		DiagramItem m;
		m.p1 = Point(2, 2);
		m.p2 = Point(23, cy - 2);
		m.width = DPI(1);
		m.shape = i;
		shape.Add(i, MakeImage(m));
	}
	shape << [=] { SetAttrs(); };
	
	struct Dialine : DiagramItem {
		Dialine() {
			shape = SHAPE_LINE;
			p1.y = p2.y = 7;
			p1.x = -9999;
			p2.x = 9999;
		}
	};
	
	auto LDL = [=](DropList& dl, bool left) {
		for(int i = DiagramItem::CAP_NONE; i < DiagramItem::CAP_COUNT; i++) {
			Dialine m;
			m.line_end = m.line_start = i;
			
			if(left)
				m.p1.x = 8;
			else
				m.p2.x = 16;
			
			dl.Add(i, MakeImage(m));
		}
		dl << [=] { SetAttrs(); };
	};
	
	LDL(line_start, true);
	LDL(line_end, false);

	for(int i = 0; i < DiagramItem::DASH_COUNT; i++) {
		Dialine m;
		m.dash = i;
		line_dash.Add(i, MakeImage(m));
	}
	line_dash << [=] { SetAttrs(); };
	
	for(int i = 0; i < 10; i++)
		line_width.Add(i);
	line_width << [=] { SetAttrs(); };

	ResetUndo();
	Sync();
	
	sb.AutoHide();
	sb.WithSizeGrip();
	AddFrame(sb);
	sb.WhenScroll << [=] { Sync(); };
	
	editor = true;
}

void DiagramEditor::Paint(Draw& w)
{
	RTIMING("Paint");
	Size sz = GetSize();
	DrawPainter iw(w, GetSize());
	iw.Co();
	iw.Clear(SWhite());

	iw.Scale(GetZoom());
	iw.Offset(-(Point)sb);
	Size dsz = data.GetSize();
	iw.Move(dsz.cx, 0).Line(dsz.cx, dsz.cy).Line(0, dsz.cy).Stroke(0.2, SColorHighlight());

	if(data.item.GetCount() == 0)
		iw.DrawText(DPI(30), DPI(30), "Right-click to insert item(s)", ArialZ(30).Italic(), SLtGray());

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
	sb.SetTotal(data.GetSize());
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
}

String DiagramEditor::GetCurrent()
{
	return StoreAsString(data);
}

bool DiagramEditor::SetCurrent(const String& s)
{
	bool b = LoadFromString(data, s);
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
	creating = false;
	ResetUndo();
	Sync();
}

}