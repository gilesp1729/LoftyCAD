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

// Indexed by enum OPERATION
char *op_string[OP_MAX] = { "U", "^", "-", " " };

// Object being highlighted by mouse over in treeview
Object *treeview_highlight;

// Limit of children, to stop ridiculously large expansions
#define TREEVIEW_LIMIT     2000

// Descriptive string for an object, to be used in the treeview, and elsewhere there is a
// need to echo out an object's description.
char *obj_description(Object *obj, char *descr, int descr_len)
{
    char buf[64], buf2[64], buf3[64];
    Point *p;
    Face *face;
    Edge *edge;
    Volume *vol;
    Group *grp;

    switch (obj->type)
    {
    case OBJ_POINT:
        p = (Point *)obj;
        sprintf_s(descr, descr_len, "Point %d (%s,%s,%s)",
                  obj->ID,
                  display_rounded(buf, p->x),
                  display_rounded(buf2, p->y),
                  display_rounded(buf3, p->z)
                  );
        break;

    case OBJ_EDGE:
        edge = (Edge *)obj;
        sprintf_s(descr, descr_len, "Edge %d %s%s %s",
                  obj->ID,
                  edgetypes[edge->type & ~EDGE_CONSTRUCTION],
                  (edge->type & EDGE_CONSTRUCTION) ? "(C)" : "",
                  get_dims_string(obj, buf)
                  );
        break;

    case OBJ_FACE:
        face = (Face *)obj;
        sprintf_s(descr, descr_len, "Face %d %s%s %s",
                  obj->ID,
                  facetypes[face->type & ~FACE_CONSTRUCTION],
                  (face->type & FACE_CONSTRUCTION) ? "(C)" : "",
                  get_dims_string(obj, buf)
                  );
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        sprintf_s(descr, descr_len, "%s Volume %d %s", op_string[vol->op], obj->ID, get_dims_string(obj, buf));
        break;

    case OBJ_GROUP:
        grp = (Group *)obj;
        if (grp->title[0] == '\0')
            sprintf_s(descr, descr_len, "%s Group %d", op_string[grp->op], obj->ID);
        else
            sprintf_s(descr, descr_len, "%s Group %d: %s", op_string[grp->op], obj->ID, grp->title);
        break;
    }

    return descr;
}

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
    Object *o;

    switch (obj->type)
    {
    case OBJ_POINT:
        tvi.pszText = obj_description(obj, descr, 64);
        tvi.cchTextMax = strlen(tvi.pszText);
        tvi.lParam = (LPARAM)obj;
        tvi.mask = TVIF_TEXT | TVIF_PARAM;
        if (is_selected_direct(obj, &o))
        {
            tvi.mask |= TVIF_STATE;
            tvi.state = TVIS_BOLD;
            tvi.stateMask = TVIS_BOLD;
        }
        tvins.item = tvi;
        tvins.hParent = hItem;
        tvins.hInsertAfter = TVI_FIRST;
        hItem = (HTREEITEM)SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvins);
        break;

    case OBJ_EDGE:
        edge = (Edge *)obj;
        tvi.pszText = obj_description(obj, descr, 64);
        tvi.cchTextMax = strlen(tvi.pszText);
        tvi.lParam = (LPARAM)obj;
        tvi.mask = TVIF_TEXT | TVIF_PARAM;
        if (is_selected_direct(obj, &o))
        {
            tvi.mask |= TVIF_STATE;
            tvi.state = TVIS_BOLD;
            tvi.stateMask = TVIS_BOLD;
        }
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
        tvi.pszText = obj_description(obj, descr, 64);
        tvi.cchTextMax = strlen(tvi.pszText);
        tvi.lParam = (LPARAM)obj;
        tvi.mask = TVIF_TEXT | TVIF_PARAM;
        if (is_selected_direct(obj, &o))
        {
            tvi.mask |= TVIF_STATE;
            tvi.state = TVIS_BOLD;
            tvi.stateMask = TVIS_BOLD;
        }
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
        tvi.pszText = obj_description(obj, descr, 64);
        tvi.cchTextMax = strlen(tvi.pszText);
        tvi.lParam = (LPARAM)obj;
        tvi.mask = TVIF_TEXT | TVIF_PARAM;
        if (is_selected_direct(obj, &o))
        {
            tvi.mask |= TVIF_STATE;
            tvi.state = TVIS_BOLD;
            tvi.stateMask = TVIS_BOLD;
        }
        tvins.item = tvi;
        tvins.hParent = hItem;
        tvins.hInsertAfter = TVI_FIRST;
        hItem = (HTREEITEM)SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvins);
        if (parent->lock < LOCK_FACES)
        {
            for (i = 0, face = (Face *)((Volume *)obj)->faces.head; face != NULL; face = (Face *)face->hdr.next, i++)
            {
                if (i < TREEVIEW_LIMIT)
                {
                    populate_treeview_object((Object *)face, parent, hItem);
                }
                else
                {
                    tvi.pszText = "(Limit on faces reached)";
                    tvi.cchTextMax = strlen(tvi.pszText);
                    tvi.lParam = (LPARAM)NULL;
                    tvi.mask = TVIF_TEXT;
                    tvins.item = tvi;
                    tvins.hParent = hItem;
                    tvins.hInsertAfter = TVI_FIRST;
                    hItem = (HTREEITEM)SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvins);
                    break;
                }
            }
        }
        break;
    }
}

// Populate a treeview item from an object list (either the object tree or a group)
void
populate_treeview_tree(Group *tree, HTREEITEM hItem)
{
    Object *obj, *o;
    TVINSERTSTRUCT tvins;
    TVITEM tvi = { 0, };
    char descr[128];
    HTREEITEM hGroup;

    for (obj = tree->obj_list.head; obj != NULL; obj = obj->next)
    {
        if (obj->type == OBJ_GROUP)
        {
            tvi.pszText = obj_description(obj, descr, 128);
            tvi.cchTextMax = strlen(tvi.pszText);
            tvi.lParam = (LPARAM)obj;
            tvi.mask = TVIF_TEXT | TVIF_PARAM;
            if (is_selected_direct(obj, &o))
            {
                tvi.mask |= TVIF_STATE;
                tvi.state = TVIS_BOLD;
                tvi.stateMask = TVIS_BOLD;
            }
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
    NMTREEVIEW *nmtv;
    Object *obj;
    POINT pt;

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

        case NM_RCLICK:
            if (treeview_highlight == NULL)
                break;
            GetCursorPos(&pt);
            contextmenu(treeview_highlight, pt);
            break;

        case TVN_SELCHANGED:
            nmtv = (NMTREEVIEW *)lParam;
            obj = (Object *)nmtv->itemNew.lParam;

#if 0 // Shift key handling is not done yet here - treat as if always shifted. Prevent illegal selections (TODO)
            if (nmtv->action == TVC_BYMOUSE && obj != NULL)
            {
                Object *o;

                if (!is_selected_direct(obj, &o))
                    link_single_checked(obj, &selection);
                else
                    remove_from_selection(obj);

                update_drawing();
            }
#endif
            break;
        }
        break;
    }

    return 0;
}

