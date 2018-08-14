#include "stdafx.h"
#include "LoftyCAD.h"
#include "windows.h"
#include "windowsx.h"
#include <CommCtrl.h>
#include <CommDlg.h>

// Externs from serialise.c
extern char *locktypes[];
extern char *edgetypes[];
extern char *facetypes[];

// Object being highlighted by mouse over in treeview
Object *treeview_highlight;

// Populate a treeview item for the components of an object.
void
populate_treeview_object(Object *obj, Object *parent, HTREEITEM hItem)
{
    TVINSERTSTRUCT tvins;
    TVITEM tvi;
    char descr[64];
    int i;
    Face *face;
    Edge *edge;

    switch (obj->type)
    {
    case OBJ_POINT:
        sprintf_s(descr, 64, "Point %d", obj->ID);
        tvi.pszText = descr;
        tvi.cchTextMax = strlen(tvi.pszText);
        tvi.lParam = (LPARAM)obj;
        tvi.mask = TVIF_TEXT | TVIF_PARAM;
        tvins.item = tvi;
        tvins.hParent = hItem;
        tvins.hInsertAfter = TVI_FIRST;
        hItem = (HTREEITEM)SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvins);
        break;

    case OBJ_EDGE:
        edge = (Edge *)obj;
        sprintf_s(descr, 64, "Edge %d %s%s",
                  obj->ID,
                  edgetypes[edge->type & ~EDGE_CONSTRUCTION],
                  (edge->type & EDGE_CONSTRUCTION) ? "(C)" : ""
                  );
        tvi.pszText = descr;
        tvi.cchTextMax = strlen(tvi.pszText);
        tvi.lParam = (LPARAM)obj;
        tvi.mask = TVIF_TEXT | TVIF_PARAM;
        tvins.item = tvi;
        tvins.hParent = hItem;
        tvins.hInsertAfter = TVI_FIRST;
        hItem = (HTREEITEM)SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvins);
        if (parent->lock < LOCK_POINTS)
        {
            populate_treeview_object((Object *)edge->endpoints[0], parent, hItem);
            populate_treeview_object((Object *)edge->endpoints[1], parent, hItem);
        }
        break;

    case OBJ_FACE:
        face = (Face *)obj;
        sprintf_s(descr, 64, "Face %d %s%s", 
                  obj->ID, 
                  facetypes[face->type & ~FACE_CONSTRUCTION],
                  (face->type & FACE_CONSTRUCTION) ? "(C)" : ""
                  );
        tvi.pszText = descr;
        tvi.cchTextMax = strlen(tvi.pszText);
        tvi.lParam = (LPARAM)obj;
        tvi.mask = TVIF_TEXT | TVIF_PARAM;
        tvins.item = tvi;
        tvins.hParent = hItem;
        tvins.hInsertAfter = TVI_FIRST;
        hItem = (HTREEITEM)SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvins);
        if (parent->lock < LOCK_EDGES)
        {
            for (i = 0; i < face->n_edges; i++)
                populate_treeview_object((Object *)face->edges[i], parent, hItem);
        }
        break;

    case OBJ_VOLUME:
        sprintf_s(descr, 64, "Volume %d", obj->ID);
        tvi.pszText = descr;
        tvi.cchTextMax = strlen(tvi.pszText);
        tvi.lParam = (LPARAM)obj;
        tvi.mask = TVIF_TEXT | TVIF_PARAM;
        tvins.item = tvi;
        tvins.hParent = hItem;
        tvins.hInsertAfter = TVI_FIRST;
        hItem = (HTREEITEM)SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvins);
        if (parent->lock < LOCK_FACES)
        {
            for (face = ((Volume *)obj)->faces.head; face != NULL; face = (Face *)face->hdr.next)
                populate_treeview_object((Object *)face, parent, hItem);
        }
        break;
    }
}

// Populate a treeview item from an object list (either the object tree or a group)
void
populate_treeview_tree(Group *tree, HTREEITEM hItem)
{
    Object *obj;
    TVINSERTSTRUCT tvins;
    TVITEM tvi = { 0, };
    char descr[128];
    HTREEITEM hGroup;

    for (obj = tree->obj_list.head; obj != NULL; obj = obj->next)
    {
        if (obj->type == OBJ_GROUP)
        {
            Group *grp = (Group *)obj;
            if (grp->title[0] == '\0')
                sprintf_s(descr, 128, "Group %d", obj->ID);
            else
                sprintf_s(descr, 128, "Group %d: %s", obj->ID, grp->title);
            tvi.pszText = descr;
            tvi.cchTextMax = strlen(tvi.pszText);
            tvi.lParam = (LPARAM)obj;
            tvi.mask = TVIF_TEXT | TVIF_PARAM;
            tvins.item = tvi;
            tvins.hParent = hItem;
            tvins.hInsertAfter = TVI_FIRST;
            hGroup = (HTREEITEM)SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvins);
            populate_treeview_tree((Group *)obj, hGroup);
        }
        else
        {
            populate_treeview_object(obj, obj, hItem);
        }
    }
}

// Populate the treeview.
void
populate_treeview(void)
{
    TVINSERTSTRUCT tvins;
    TVITEM tvi;
    HTREEITEM hRoot;

    // Delete the existing treeview items
    //TreeView_DeleteAllItems(hWndTree);  // this doesn't work, for some reason
    SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);

    // Put in the root item
    tvi.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM;
    if (object_tree.title[0] == '\0')
        tvi.pszText = "Tree";
    else
        tvi.pszText = object_tree.title;
    tvi.cchTextMax = strlen(tvi.pszText);
    tvi.state = TVIS_EXPANDED;
    tvi.stateMask = TVIS_EXPANDED;
    tvi.lParam = (LPARAM)NULL;
    tvins.item = tvi;
    tvins.hParent = NULL;
    tvins.hInsertAfter = TVI_ROOT;
    hRoot = (HTREEITEM)SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvins);

    // Populate the rest of the tree view
    populate_treeview_tree(&object_tree, hRoot);
}

// Wndproc for tree view dialog.
int WINAPI
treeview_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HMENU hMenu;
    NMTVGETINFOTIP *ngit;
    // NMTREEVIEW *nmtv;
    // Object *obj, *sel_obj;

    switch (msg)
    {
    case WM_CLOSE:
        treeview_highlight = NULL;
        view_tree = FALSE;
        ShowWindow(hWnd, SW_HIDE);
        hMenu = GetSubMenu(GetMenu(auxGetHWND()), 2);
        CheckMenuItem(hMenu, ID_VIEW_TREE, view_tree ? MF_CHECKED : MF_UNCHECKED);
        break;

    case WM_NOTIFY:
        switch (((NMHDR *)lParam)->code)
        {
        case TVN_GETINFOTIP:
            ngit = (NMTVGETINFOTIP *)lParam;
            treeview_highlight = (Object *)ngit->lParam;
            break;
#if 0 // Don't do this - it needs too much work (e.g. handling clicks in the main window, shift, etc.)
        case TVN_SELCHANGED:
            nmtv = (NMTREEVIEW *)lParam;
            obj = (Object *)nmtv->itemNew.lParam;

            if (obj != NULL)
                link_single(obj, &selection);
            break;
#endif
        }
        break;
    }

    return 0;
}

