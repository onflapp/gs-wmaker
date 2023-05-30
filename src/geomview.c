
#include <WINGs/WINGsP.h>

#include "geomview.h"


struct W_GeometryView {
	W_Class widgetClass;
	WMView *view;

	WMColor *color;
	WMColor *bgColor;

	WMFont *font;

	WMSize textSize;

	union {
		struct {
			int x, y;
		} pos;
		struct {
			unsigned width, height;
		} size;
	} data;

	unsigned showPosition:1;
};

static void handleEvents(XEvent * event, void *clientData);
static void paint(WGeometryView * gview);

WGeometryView *WCreateGeometryView(WMScreen * scr)
{
	WGeometryView *gview;
	char buffer[64];
	static W_Class widgetClass = 0;

	if (!widgetClass) {
		widgetClass = W_RegisterUserWidget();
	}

	gview = malloc(sizeof(WGeometryView));
	if (!gview) {
		return NULL;
	}
	memset(gview, 0, sizeof(WGeometryView));

	gview->widgetClass = widgetClass;

	gview->view = W_CreateTopView(scr);
	if (!gview->view) {
		wfree(gview);

		return NULL;
	}
	gview->view->self = gview;

	gview->font = WMSystemFontOfSize(scr, 12);
	if (!gview->font) {
		W_DestroyView(gview->view);
		wfree(gview);

		return NULL;
	}

	gview->bgColor = WMCreateRGBColor(scr, 0x3333, 0x6666, 0x9999, True);
	gview->color = WMWhiteColor(scr);

	WMCreateEventHandler(gview->view, ExposureMask, handleEvents, gview);
	W_ResizeView(gview->view, gview->textSize.width + 8, gview->textSize.height + 6);

	return gview;
}

void WSetGeometryViewShownPosition(WGeometryView * gview, int x, int y)
{
	gview->showPosition = 1;
	gview->data.pos.x = x;
	gview->data.pos.y = y;

}

void WSetGeometryViewShownSize(WGeometryView * gview, unsigned width, unsigned height)
{
	gview->showPosition = 0;
	gview->data.size.width = width;
	gview->data.size.height = height;

}


static void handleEvents(XEvent * event, void *clientData)
{
	WGeometryView *gview = (WGeometryView *) clientData;

	switch (event->type) {
	case Expose:
		return;
		break;

	}
}
