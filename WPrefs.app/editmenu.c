/* editmenu.c - editable menus
 * 
 *  WPrefs - Window Maker Preferences Program
 * 
 *  Copyright (c) 2000 Alfredo K. Kojima
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 */


#include <WINGsP.h>
#include <WUtil.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "editmenu.h"

typedef struct W_EditMenuItem {
    W_Class widgetClass;
    WMView *view;

    struct W_EditMenu *parent;

    char *label;

    void *data;
    WMCallback *destroyData;

    struct W_EditMenu *submenu;	       /* if it's a cascade, NULL otherwise */
    
    struct {
	unsigned isTitle:1;
	unsigned isHighlighted:1;
    } flags;
} EditMenuItem;


typedef struct W_EditMenu {
    W_Class widgetClass;
    WMView *view;

    struct W_EditMenu *parent;

    WMBag *items; /* EditMenuItem */
    
    EditMenuItem *selectedItem;
    
    WMTextField *tfield;

    int titleHeight;
    int itemHeight;
    
    WEditMenuDelegate *delegate;

    /* item dragging */
    int draggedItem;
    int dragX, dragY;
    
    struct W_EditMenu **dropTargets;
    
    /* only for non-standalone menu */
    WMSize maxSize;
    WMSize minSize;
    
    struct {
	unsigned standalone:1;
	unsigned isTitled:1;
	
	unsigned isFactory:1;
	unsigned isSelectable:1;
	unsigned isEditable:1;

	unsigned isTornOff:1;
	
	unsigned isDragging:1;
	unsigned isEditing:1;
    } flags;
} EditMenu;



/******************** WEditMenuItem ********************/

static void destroyEditMenuItem(WEditMenuItem *iPtr);
static void paintEditMenuItem(WEditMenuItem *iPtr);
static void handleItemEvents(XEvent *event, void *data);

static void handleItemClick(XEvent *event, void *data);


static W_Class EditMenuItemClass = 0;


W_Class
InitEditMenuItem(WMScreen *scr)
{
    /* register our widget with WINGs and get our widget class ID */
    if (!EditMenuItemClass) {
	EditMenuItemClass = W_RegisterUserWidget();
    }

    return EditMenuItemClass;
}


WEditMenuItem*
WCreateEditMenuItem(WMWidget *parent, char *title, Bool isTitle)
{
    WEditMenuItem *iPtr;
    WMScreen *scr = WMWidgetScreen(parent);

    if (!EditMenuItemClass)
	InitEditMenuItem(scr);


    iPtr = wmalloc(sizeof(WEditMenuItem));

    memset(iPtr, 0, sizeof(WEditMenuItem));

    iPtr->widgetClass = EditMenuItemClass;
    
    iPtr->view = W_CreateView(W_VIEW(parent));
    if (!iPtr->view) {
	free(iPtr);
	return NULL;
    }
    iPtr->view->self = iPtr;

    iPtr->parent = parent;
    
    iPtr->label = wstrdup(title);
    
    iPtr->flags.isTitle = isTitle;
    
    if (isTitle) {
	WMSetWidgetBackgroundColor(iPtr, WMBlackColor(scr));
    }
    
    WMCreateEventHandler(iPtr->view, ExposureMask|StructureNotifyMask,
			 handleItemEvents, iPtr);
    
    WMCreateEventHandler(iPtr->view, ButtonPressMask|ButtonReleaseMask
			 |ButtonMotionMask, handleItemClick, iPtr);


    return iPtr;
}


void *WGetEditMenuItemData(WEditMenuItem *item)
{
    return item->data;
}


void WSetEditMenuItemData(WEditMenuItem *item, void *data, 
			   WMCallback *destroyer)
{
    item->data = data;
    item->destroyData = destroyer;
}




static void
paintEditMenuItem(WEditMenuItem *iPtr)
{
    WMScreen *scr = WMWidgetScreen(iPtr);
    WMColor *color;
    Window win = W_VIEW(iPtr)->window;
    int w = W_VIEW(iPtr)->size.width;
    int h = W_VIEW(iPtr)->size.height;
    WMFont *font = (iPtr->flags.isTitle ? scr->boldFont : scr->normalFont);

    if (!iPtr->view->flags.realized)
	return;
    
    color = scr->black;
    if (iPtr->flags.isTitle && !iPtr->flags.isHighlighted) {
	color = scr->white;
    }
    

    XClearWindow(scr->display, win);

    W_DrawRelief(scr, win, 0, 0, w+1, h, WRRaised);

    WMDrawString(scr, win, WMColorGC(color), font, 5, 3, iPtr->label,
		 strlen(iPtr->label));
    
    if (iPtr->submenu) {
	/* draw the cascade indicator */
	XDrawLine(scr->display,win,WMColorGC(scr->darkGray), 
		  w-11, 6, w-6, h/2-1);
	XDrawLine(scr->display,win,WMColorGC(scr->white),
		  w-11, h-8, w-6, h/2-1);
	XDrawLine(scr->display,win,WMColorGC(scr->black), 
		  w-12, 6, w-12, h-8);
    }
}


static void
highlightItem(WEditMenuItem *iPtr, Bool high)
{   
    if (iPtr->flags.isTitle)
	return;
    
    iPtr->flags.isHighlighted = high;

    if (high) {
	WMSetWidgetBackgroundColor(iPtr, WMWhiteColor(WMWidgetScreen(iPtr)));
    } else {
	if (!iPtr->flags.isTitle) {
	    WMSetWidgetBackgroundColor(iPtr,
				       WMGrayColor(WMWidgetScreen(iPtr)));
	} else {
	    WMSetWidgetBackgroundColor(iPtr,
				       WMBlackColor(WMWidgetScreen(iPtr)));
	}
    }
}


static int
getItemTextWidth(WEditMenuItem *iPtr)
{
    WMScreen *scr = WMWidgetScreen(iPtr);
    
    if (iPtr->flags.isTitle) {
	return WMWidthOfString(scr->boldFont, iPtr->label, 
			       strlen(iPtr->label));
    } else {
	return WMWidthOfString(scr->normalFont, iPtr->label, 
			       strlen(iPtr->label));
    }
}



static void
handleItemEvents(XEvent *event, void *data)
{
    WEditMenuItem *iPtr = (WEditMenuItem*)data;

    switch (event->type) {	
     case Expose:
	if (event->xexpose.count!=0)
	    break;
	paintEditMenuItem(iPtr);
	break;

     case DestroyNotify:
	destroyEditMenuItem(iPtr);
	break;
    }
}


static void
destroyEditMenuItem(WEditMenuItem *iPtr)
{
    if (iPtr->label)
	free(iPtr->label);
    if (iPtr->data && iPtr->destroyData)
	(*iPtr->destroyData)(iPtr->data);

    free(iPtr);
}



/******************** WEditMenu *******************/

static void destroyEditMenu(WEditMenu *mPtr);

static void updateMenuContents(WEditMenu *mPtr);

static void handleEvents(XEvent *event, void *data);
static void handleActionEvents(XEvent *event, void *data);

static void editItemLabel(WEditMenuItem *item);
static void stopEditItem(WEditMenu *menu, Bool apply);


static W_Class EditMenuClass = 0;


W_Class
InitEditMenu(WMScreen *scr)
{
    /* register our widget with WINGs and get our widget class ID */
    if (!EditMenuClass) {

	EditMenuClass = W_RegisterUserWidget();
    }

    return EditMenuClass;
}



typedef struct {
    int flags;
    int window_style;
    int window_level;
    int reserved;
    Pixmap miniaturize_pixmap;         /* pixmap for miniaturize button */
    Pixmap close_pixmap;               /* pixmap for close button */
    Pixmap miniaturize_mask;           /* miniaturize pixmap mask */
    Pixmap close_mask;                 /* close pixmap mask */
    int extra_flags;
} GNUstepWMAttributes;


#define GSWindowStyleAttr       (1<<0)
#define GSWindowLevelAttr       (1<<1)


static void
writeGNUstepWMAttr(WMScreen *scr, Window window, GNUstepWMAttributes *attr)
{
    unsigned long data[9];
        
    /* handle idiot compilers where array of CARD32 != struct of CARD32 */
    data[0] = attr->flags;
    data[1] = attr->window_style;
    data[2] = attr->window_level;
    data[3] = 0;                       /* reserved */
    /* The X protocol says XIDs are 32bit */
    data[4] = attr->miniaturize_pixmap;
    data[5] = attr->close_pixmap;
    data[6] = attr->miniaturize_mask;
    data[7] = attr->close_mask;
    data[8] = attr->extra_flags;
    XChangeProperty(scr->display, window, scr->attribsAtom, scr->attribsAtom,
                    32, PropModeReplace,  (unsigned char *)data, 9);
}


static void
realizeObserver(void *self, WMNotification *not)
{
    WEditMenu *menu = (WEditMenu*)self;
    GNUstepWMAttributes attribs;
    
    memset(&attribs, 0, sizeof(GNUstepWMAttributes));
    attribs.flags = GSWindowStyleAttr|GSWindowLevelAttr;
    attribs.window_style = WMBorderlessWindowMask;
    attribs.window_level = WMSubmenuWindowLevel;

    writeGNUstepWMAttr(WMWidgetScreen(menu), menu->view->window, &attribs);
}


static WEditMenu*
makeEditMenu(WMScreen *scr, WMWidget *parent, char *title)
{
    WEditMenu *mPtr;
    WEditMenuItem *item;

    if (!EditMenuClass)
	InitEditMenu(scr);


    mPtr = wmalloc(sizeof(WEditMenu));
    memset(mPtr, 0, sizeof(WEditMenu));

    mPtr->widgetClass = EditMenuClass;

    if (parent) {
	mPtr->view = W_CreateView(W_VIEW(parent));
	mPtr->flags.standalone = 0;
    } else {
	mPtr->view = W_CreateTopView(scr);
	mPtr->flags.standalone = 1;
    }
    if (!mPtr->view) {
	free(mPtr);
	return NULL;
    }
    mPtr->view->self = mPtr;

    mPtr->flags.isSelectable = 1;
    mPtr->flags.isEditable = 1;

    W_SetViewBackgroundColor(mPtr->view, scr->darkGray);

    WMAddNotificationObserver(realizeObserver, mPtr,
                              WMViewRealizedNotification, mPtr->view);

    mPtr->items = WMCreateBag(4);

    WMCreateEventHandler(mPtr->view, ExposureMask|StructureNotifyMask,
			 handleEvents, mPtr);

    WMCreateEventHandler(mPtr->view, ButtonPressMask,handleActionEvents, mPtr);
    

    if (title != NULL) {
	item = WCreateEditMenuItem(mPtr, title, True);

	WMMapWidget(item);
	WMPutInBag(mPtr->items, item);

	mPtr->flags.isTitled = 1;
    }

    mPtr->itemHeight = WMFontHeight(scr->normalFont) + 6;
    mPtr->titleHeight = WMFontHeight(scr->boldFont) + 8;

    updateMenuContents(mPtr);
    
    return mPtr;
}


WEditMenu*
WCreateEditMenu(WMScreen *scr, char *title)
{
    return makeEditMenu(scr, NULL, title);
}


WEditMenu*
WCreateEditMenuPad(WMWidget *parent)
{
    return makeEditMenu(WMWidgetScreen(parent), parent, NULL);
}


void
WSetEditMenuDelegate(WEditMenu *mPtr, WEditMenuDelegate *delegate)
{
    mPtr->delegate = delegate;
}


WEditMenuItem*
WInsertMenuItemWithTitle(WEditMenu *mPtr, int index, char *title)
{
    WEditMenuItem *item;

    item = WCreateEditMenuItem(mPtr, title, False);

    WMMapWidget(item);
    
    if (index >= WMGetBagItemCount(mPtr->items))
	WMPutInBag(mPtr->items, item);
    else {
	if (index < 0)
	    index = 0;
	if (mPtr->flags.isTitled)
	    index++;
	WMInsertInBag(mPtr->items, index, item);
    }

    updateMenuContents(mPtr);

    return item;
}



WEditMenuItem*
WAddMenuItemWithTitle(WEditMenu *mPtr, char *title)
{
    return WInsertMenuItemWithTitle(mPtr, WMGetBagItemCount(mPtr->items),
				    title);
}


void
WSetEditMenuItemDropTargets(WEditMenu *mPtr, WEditMenu **dropTargets, 
			    int count)
{
    if (mPtr->dropTargets)
	free(mPtr->dropTargets);
    
    mPtr->dropTargets = wmalloc(sizeof(WEditMenu*)*(count+1));
    memcpy(mPtr->dropTargets, dropTargets, sizeof(WEditMenu*)*count);
    mPtr->dropTargets[count] = NULL;
}


void
WSetEditMenuSubmenu(WEditMenu *mPtr, WEditMenuItem *item, WEditMenu *submenu)
{
    item->submenu = submenu;
    submenu->parent = mPtr;

    paintEditMenuItem(item);
}

static int simpleMatch(void *a, void *b)
{
    if (a == b)
	return 1;
    else
	return 0;
}


void
WRemoveEditMenuItem(WEditMenu *mPtr, WEditMenuItem *item)
{
    int index;

    index = WMFindInBag(mPtr->items, simpleMatch, item);
    
    if (index == WBNotFound) {
	return;
    }
    
    WMDeleteFromBag(mPtr->items, index);

    updateMenuContents(mPtr);
}


void
WSetEditMenuSelectable(WEditMenu *mPtr, Bool flag)
{
    mPtr->flags.isSelectable = flag;
}


void
WSetEditMenuEditable(WEditMenu *mPtr, Bool flag)
{
    mPtr->flags.isEditable = flag;
}


void
WSetEditMenuIsFactory(WEditMenu *mPtr, Bool flag)
{
    mPtr->flags.isFactory = flag;
}


void
WSetEditMenuMinSize(WEditMenu *mPtr, WMSize size)
{
    mPtr->minSize.width = size.width;
    mPtr->minSize.height = size.height;
}


void
WSetEditMenuMaxSize(WEditMenu *mPtr, WMSize size)
{
    mPtr->maxSize.width = size.width;
    mPtr->maxSize.height = size.height;
}


WMPoint
WGetEditMenuLocationForSubmenu(WEditMenu *mPtr, WEditMenu *submenu)
{
    WMBagIterator iter;
    WEditMenuItem *item;
    int y;
    
    if (mPtr->flags.isTitled) {
	y = mPtr->titleHeight;
    } else {
	y = 0;
    }
    WM_ITERATE_BAG(mPtr->items, item, iter) {
	if (item->submenu == submenu) {
	    return wmkpoint(mPtr->view->pos.x + mPtr->view->size.width, y);
	}
	y += mPtr->itemHeight;
    }

    puts("invalid submenu passed to WGetEditMenuLocationForSubmenu()");
    
    return wmkpoint(0,0);
}


Bool
WEditMenuIsTornOff(WEditMenu *mPtr)
{
    return !mPtr->flags.isTornOff;
}


static void
updateMenuContents(WEditMenu *mPtr)
{
    int newW, newH;
    int w;
    int i;
    int iheight = mPtr->itemHeight;
    int offs = 1;
    WMBagIterator iter;
    WEditMenuItem *item;

    newW = 0;
    newH = offs;

    i = 0;
    WM_ITERATE_BAG(mPtr->items, item, iter) {	
	w = getItemTextWidth(item);

	newW = WMAX(w, newW);

	WMMoveWidget(item, offs, newH);
	if (i == 0 && mPtr->flags.isTitled) {
	    newH += mPtr->titleHeight;
	} else {
	    newH += iheight;
	}
	i = 1;
    }

    newW += iheight + 5;
    newH--;
    
    if (mPtr->minSize.width)
	newW = WMAX(newW, mPtr->minSize.width);
    if (mPtr->maxSize.width)
	newW = WMIN(newW, mPtr->maxSize.width); 
   
    if (mPtr->minSize.height)
	newH = WMAX(newH, mPtr->minSize.height);
    if (mPtr->maxSize.height)
	newH = WMIN(newH, mPtr->maxSize.height);

    W_ResizeView(mPtr->view, newW, newH+1);

    newW -= 2*offs;
    
    i = 0;
    WM_ITERATE_BAG(mPtr->items, item, iter) {
	if (i == 0 && mPtr->flags.isTitled) {
	    WMResizeWidget(item, newW, mPtr->titleHeight);	    
	} else {
	    WMResizeWidget(item, newW, iheight);
	}
	i = 1;
    }
}


static void
selectItem(WEditMenu *menu, WEditMenuItem *item)
{
    if (!menu->flags.isSelectable) {
	return;
    }

    if (menu->selectedItem) {
	highlightItem(menu->selectedItem, False);

	if (menu->delegate) {
	    (*menu->delegate->itemDeselected)(menu->delegate, menu, 
					      menu->selectedItem);
	}
    }

    if (menu->flags.isEditing) {
	stopEditItem(menu, False);
    }

    if (item) {
	highlightItem(item, True);

	if (item->submenu) {
	    WMPoint pt;
	    
	    pt = WGetEditMenuLocationForSubmenu(menu, item->submenu);
	    WMMoveWidget(item->submenu, pt.x, pt.y);
	    WMMapWidget(item->submenu);
	}
	
	if (menu->delegate) {
	    (*menu->delegate->itemSelected)(menu->delegate, menu, item);
	}
    }

    menu->selectedItem = item;
}


static void
paintMenu(WEditMenu *mPtr)
{
    W_View *view = mPtr->view;
    
    W_DrawRelief(W_VIEW_SCREEN(view), W_VIEW_DRAWABLE(view), 0, 0,
		 W_VIEW_WIDTH(view), W_VIEW_HEIGHT(view), WRSimple);
}


static void
handleEvents(XEvent *event, void *data)
{
    WEditMenu *mPtr = (WEditMenu*)data;

    switch (event->type) {	
     case DestroyNotify:
	destroyEditMenu(mPtr);
	break;
	
     case Expose:
	if (event->xexpose.count == 0)
	    paintMenu(mPtr);
	break;
    }
}




static void
handleActionEvents(XEvent *event, void *data)
{
//    WEditMenu *mPtr = (WEditMenu*)data;

    switch (event->type) {
     case ButtonPress:
	break;
    }
}


/* -------------------------- Menu Label Editing ------------------------ */


static void
stopEditItem(WEditMenu *menu, Bool apply)
{
    char *text;
    
    if (apply) {
	text = WMGetTextFieldText(menu->tfield);
	
	free(menu->selectedItem->label);
	menu->selectedItem->label = wstrdup(text);

	updateMenuContents(menu);
    } 
    WMUnmapWidget(menu->tfield);
    menu->flags.isEditing = 0;
}


static void
textEndedEditing(struct WMTextFieldDelegate *self, WMNotification *notif)
{
    WEditMenu *menu = (WEditMenu*)self->data;
    int reason;
    int i;
    WEditMenuItem *item;

    if (!menu->flags.isEditing)
	return;

    reason = (int)WMGetNotificationClientData(notif);
    
    switch (reason) {	
     case WMEscapeTextMovement:
	stopEditItem(menu, False);
	break;
	
     case WMReturnTextMovement:
	stopEditItem(menu, True);
	break;
	
     case WMTabTextMovement:
	stopEditItem(menu, True);

	i = WMFindInBag(menu->items, simpleMatch, menu->selectedItem);
	item = WMGetFromBag(menu->items, i+1);
	if (item != NULL) {
	    selectItem(menu, item);
	    editItemLabel(item);
	}
	break;
	
     case WMBacktabTextMovement:
	stopEditItem(menu, True);

	i = WMFindInBag(menu->items, simpleMatch, menu->selectedItem);
	item = WMGetFromBag(menu->items, i-1);
	if (item != NULL) {
	    selectItem(menu, item);
	    editItemLabel(item);
	}
	break;
    }
}



static WMTextFieldDelegate textFieldDelegate = {
    NULL,
	NULL,
	NULL,
	textEndedEditing,
	NULL,
	NULL
};


static void
editItemLabel(WEditMenuItem *item)
{
    WEditMenu *mPtr = item->parent;
    WMTextField *tf;
    int i;
    
    if (!mPtr->flags.isEditable) {
	return;
    }

    if (!mPtr->tfield) {
	mPtr->tfield = WMCreateTextField(mPtr);
	WMSetTextFieldBeveled(mPtr->tfield, False);
	WMRealizeWidget(mPtr->tfield);
	
	textFieldDelegate.data = mPtr;

	WMSetTextFieldDelegate(mPtr->tfield, &textFieldDelegate);
    }
    tf = mPtr->tfield;

    i = WMFindInBag(mPtr->items, simpleMatch, item);

    WMSetTextFieldText(tf, item->label);
    WMSelectTextFieldRange(tf, wmkrange(0, strlen(item->label)));
    WMResizeWidget(tf, mPtr->view->size.width, mPtr->itemHeight);
    WMMoveWidget(tf, 0, item->view->pos.y);
    WMMapWidget(tf);
    WMSetFocusToWidget(tf);
    
    mPtr->flags.isEditing = 1;
}



/* -------------------------------------------------- */


static void
slideWindow(Display *dpy, Window win, int srcX, int srcY, int dstX, int dstY)
{
    double x, y, dx, dy;
    int i;
    int iterations;

    iterations = WMIN(25, WMAX(abs(dstX-srcX), abs(dstY-srcY)));
    
    x = srcX;
    y = srcY;
    
    dx = (double)(dstX-srcX)/iterations;
    dy = (double)(dstY-srcY)/iterations;

    for (i = 0; i <= iterations; i++) {
	XMoveWindow(dpy, win, x, y);
        XFlush(dpy);
        
	wusleep(800);
	
        x += dx;
        y += dy;
    }
}


static WEditMenu*
getEditMenuForWindow(WEditMenu **menus, Window window)
{
    while (*menus) {
	if (W_VIEW_DRAWABLE((*menus)->view) == window) {
	    return *menus;
	}
	menus++;
    }
    
    return NULL;
}


static WEditMenu*
findMenuInWindow(Display *dpy, Window toplevel, int x, int y,
		  WEditMenu **menus)
{
    Window foo, bar;
    Window *children;
    unsigned nchildren;
    int i;
    WEditMenu *menu;

    menu = getEditMenuForWindow(menus, toplevel);
    if (menu)
	return menu;
    
    if (!XQueryTree(dpy, toplevel, &foo, &bar,
		    &children, &nchildren) || children == NULL) {
	return NULL;
    }
    
    /* first window that contains the point is the one */
    for (i = nchildren-1; i >= 0; i--) {
	XWindowAttributes attr;

	if (XGetWindowAttributes(dpy, children[i], &attr)
	    && attr.map_state == IsViewable
	    && x >= attr.x && y >= attr.y
	    && x < attr.x + attr.width && y < attr.y + attr.height) {
	    Window tmp;

	    tmp = children[i];

	    menu = findMenuInWindow(dpy, tmp, x - attr.x, y - attr.y, menus);
	    if (menu) {
		XFree(children);
		return menu;
	    }
	}
    }

    XFree(children);
    return NULL;
}


static void
handleDragOver(WEditMenu *menu, WMView *view, WEditMenuItem *item, 
	       int x, int y)
{
    WMScreen *scr = W_VIEW_SCREEN(menu->view);
    Window blaw;
    int mx, my;
    int offs;

    XTranslateCoordinates(scr->display, W_VIEW_DRAWABLE(menu->view),
			  scr->rootWin, 0, 0, &mx, &my, &blaw);
    
    offs = menu->flags.standalone ? 0 : 1;
    
    W_MoveView(view, mx + offs, y);
    if (view->size.width != menu->view->size.width) {
	W_ResizeView(view, menu->view->size.width - 2*offs, 
		     menu->itemHeight);
	W_ResizeView(item->view, menu->view->size.width - 2*offs,
		     menu->itemHeight);
    }
}


static void
handleItemDrop(WEditMenu *menu, WEditMenuItem *item, int x, int y)
{ 
    WMScreen *scr = W_VIEW_SCREEN(menu->view);
    Window blaw;
    int mx, my;
    int index;

    XTranslateCoordinates(scr->display, W_VIEW_DRAWABLE(menu->view),
			  scr->rootWin, 0, 0, &mx, &my, &blaw);

    index = y - my;
    if (menu->flags.isTitled) {
	index -= menu->titleHeight;
    }
    index = (index + menu->itemHeight/2) / menu->itemHeight;
    if (index < 0)
	index = 0;

    if (menu->flags.isTitled) {
	index++;
    }
    
    if (index > WMGetBagItemCount(menu->items)) {
	WMPutInBag(menu->items, item);
    } else {
	WMInsertInBag(menu->items, index, item);
    }

    W_ReparentView(item->view, menu->view, 0, index*menu->itemHeight);
    
    item->parent = menu;

    updateMenuContents(menu);
}


static void
dragMenu(WEditMenu *menu)
{
    WMScreen *scr = W_VIEW_SCREEN(menu->view);
    XEvent ev;
    Bool done = False;
    int dx, dy;
    unsigned blau;
    Window blaw;
    
    XGetGeometry(scr->display, W_VIEW_DRAWABLE(menu->view), &blaw, &dx, &dy,
		 &blau, &blau, &blau, &blau);
    XTranslateCoordinates(scr->display, W_VIEW_DRAWABLE(menu->view),
			  scr->rootWin, dx, dy, &dx, &dy, &blaw);
    dx = menu->dragX - dx;
    dy = menu->dragY - dy;

    XGrabPointer(scr->display, scr->rootWin, False,
		 ButtonReleaseMask|ButtonMotionMask,
		 GrabModeAsync, GrabModeAsync, None, scr->defaultCursor,
		 CurrentTime);

    while (!done) {
	WMNextEvent(scr->display, &ev);
	
	switch (ev.type) {
	 case ButtonRelease:
	    done = True;
	    break;

	 case MotionNotify:
	    WMMoveWidget(menu, ev.xmotion.x_root - dx,
			 ev.xmotion.y_root - dy);
	    break;

	 default:
	    WMHandleEvent(&ev);
	    break;
	}
    }

    XUngrabPointer(scr->display, CurrentTime);
}



static void
dragItem(WEditMenu *menu, WEditMenuItem *item)
{
    Display *dpy = W_VIEW_DISPLAY(menu->view);
    WMScreen *scr = W_VIEW_SCREEN(menu->view);
    int x, y;
    int dx, dy;
    Bool done = False;
    Window blaw;
    int blai;
    unsigned blau;
    Window win;
    WMView *dview;
    int orix, oriy;
    Bool enteredMenu = False;
    WMSize oldSize = item->view->size;
    WEditMenu *dmenu = NULL;

    
    if (item->flags.isTitle) {
	WMRaiseWidget(menu);

	dragMenu(menu);

	return;
    }

    
    selectItem(menu, NULL);

    assert(menu->dropTargets != NULL);


    win = scr->rootWin;
    
    XTranslateCoordinates(dpy, W_VIEW_DRAWABLE(item->view), win,
			  0, 0, &orix, &oriy, &blaw);

    dview = W_CreateUnmanagedTopView(scr);
    W_SetViewBackgroundColor(dview, scr->black);
    W_ResizeView(dview, W_VIEW_WIDTH(item->view), W_VIEW_HEIGHT(item->view));
    W_MoveView(dview, orix, oriy);
    W_RealizeView(dview);
    
    if (menu->flags.isFactory) {
	WEditMenuItem *nitem;

	nitem = WCreateEditMenuItem(menu, item->label, False);
	
	if (menu->delegate) {
	    (*menu->delegate->itemCloned)(menu->delegate, menu, item, nitem);
	}
	item = nitem;
	
	W_ReparentView(item->view, dview, 0, 0);
	WMResizeWidget(item, oldSize.width, oldSize.height);
	WMRealizeWidget(item);
	WMMapWidget(item);
    } else {
	W_ReparentView(item->view, dview, 0, 0);
    }

    W_MapView(dview);

    dx = menu->dragX - orix;
    dy = menu->dragY - oriy;
    
    XGrabPointer(dpy, scr->rootWin, False,
		 ButtonPressMask|ButtonReleaseMask|ButtonMotionMask,
		 GrabModeAsync, GrabModeAsync, None, scr->defaultCursor,
		 CurrentTime);

    while (!done) {
	XEvent ev;

	WMNextEvent(dpy, &ev);

	switch (ev.type) {
	 case MotionNotify:
	    XQueryPointer(dpy, win, &blaw, &blaw, &blai, &blai, &x, &y, &blau);

	    dmenu = findMenuInWindow(dpy, win, x, y, menu->dropTargets);
		
	    if (dmenu) {
		handleDragOver(dmenu, dview, item, x - dx, y - dy);
		enteredMenu = True;
	    } else {
		if (enteredMenu) {
		    W_ResizeView(dview, oldSize.width, oldSize.height);
		    W_ResizeView(item->view, oldSize.width, oldSize.height);
		    enteredMenu = False;
		}
		W_MoveView(dview, x - dx, y - dy);
	    }
	    
	    break;
	    
	 case ButtonRelease:
	    done = True;
	    break;
	    
	 default:
	    WMHandleEvent(&ev);
	    break;
	}
    }
    XUngrabPointer(dpy, CurrentTime);

    if (menu->flags.isFactory && !enteredMenu) {
	slideWindow(dpy, W_VIEW_DRAWABLE(dview), x-dx, y-dy, orix, oriy);
    } else {
	WRemoveEditMenuItem(menu, item);
	if (enteredMenu) {
	    handleItemDrop(dmenu, item, x-dy, y-dy);
	}
    }

    /* can't destroy now because we're being called from 
     * the event handler of the item. and destroying now,
     * would mean destroying the item too in some cases.
     */
    WMAddIdleHandler((WMCallback*)W_DestroyView, dview);
}



static void
handleItemClick(XEvent *event, void *data)
{
    WEditMenuItem *item = (WEditMenuItem*)data;
    WEditMenu *menu = item->parent;

    switch (event->type) {
     case ButtonPress:
	selectItem(menu, item);

	if (WMIsDoubleClick(event)) {
	    editItemLabel(item);
	}
	
	menu->flags.isDragging = 1;
	menu->dragX = event->xbutton.x_root;
	menu->dragY = event->xbutton.y_root;
	break;
	
     case ButtonRelease:
	menu->flags.isDragging = 0;
	break;
	
     case MotionNotify:
	if (menu->flags.isDragging) {
	    if (abs(event->xbutton.x_root - menu->dragX) > 5
		|| abs(event->xbutton.y_root - menu->dragY) > 5) {
		menu->flags.isDragging = 0;
		dragItem(menu, item);
	    }
	}
	break;
    }
}


static void
destroyEditMenu(WEditMenu *mPtr)
{
    WMRemoveNotificationObserver(mPtr);

    WMFreeBag(mPtr->items);
    
    free(mPtr->dropTargets);

    free(mPtr);
}



