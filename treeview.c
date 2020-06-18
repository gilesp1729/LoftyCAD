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

// Descriptive string for a transform, to be used in an object description.
char* xform_string(Transform* xform, char* buf, int len)
{
    int n;

    buf[0] = '\0';
    if (xform == NULL)
        return buf;

    strcpy_s(buf, len, "[");
    n = 1;
    if (xform->enable_scale)
        n += sprintf_s(&buf[n], len - n, "S(%.2f, %.2f, %.2f)", xform->sx, xform->sy, xform->sz);
    if (xform->enable_rotation && n < len)  // assumes n doesn't hit len
    {
        if (xform->enable_scale)
            n += sprintf_s(&buf[n], len - n, ", ");
        n += sprintf_s(&buf[n], len - n, "R(%.1f, %.1f, %.1f)", xform->rx, xform->ry, xform->rz);
    }
    if (n < len)
        strcpy_s(&buf[n], len - n, "]");

    return buf;
}

// Descriptive string for a face or arc-edge normal.
char* normal_string(Plane *norm, char* buf, int len)
{
    buf[0] = '\0';
    if (norm == NULL)
        return buf;

    sprintf_s(buf, len, "N(%.2f, %.2f, %.2f)", norm->A, norm->B, norm->C);

    return buf;
}

// Descriptive string for a face initial point.
char* ip_string(Face *f, char* buf, int len)
{
    buf[0] = '\0';
    if (f == NULL || f->initial_point == NULL)
        return buf;

    sprintf_s(buf, len, "IP%d", f->initial_point->hdr.ID);

    return buf;
}

// Descriptive string for an object, to be used in the treeview, and elsewhere there is a
// need to echo out an object's description.
char *obj_description(Object *obj, char *descr, int descr_len, BOOL verbose)
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
        sprintf_s(descr, descr_len, "Edge %d %s%s %s %s %s",
                  obj->ID,
                  edgetypes[edge->type & ~EDGE_CONSTRUCTION],
                  (edge->type & EDGE_CONSTRUCTION) ? "(C)" : "",
                  verbose ? get_dims_string(obj, buf) : "",
                  verbose && edge->type == EDGE_ARC ? (((ArcEdge*)edge)->clockwise ? "C" : "AC") : "",
                  verbose && edge->type == EDGE_ARC ? normal_string(&((ArcEdge *)edge)->normal, buf2, 64) : ""
                  );
        break;

    case OBJ_FACE:
        face = (Face *)obj;
        sprintf_s(descr, descr_len, "Face %d %s%s %s %s %s",
                  obj->ID,
                  facetypes[face->type & ~FACE_CONSTRUCTION],
                  (face->type & FACE_CONSTRUCTION) ? "(C)" : "",
                  verbose ? get_dims_string(obj, buf) : "",
                  verbose ? ip_string(face, buf2, 64) : "",
                  verbose && face->type != FACE_CYLINDRICAL ? normal_string(&face->normal, buf3, 64) : ""
                  );
        break;

    case OBJ_VOLUME:
        vol = (Volume *)obj;
        sprintf_s(descr, descr_len, "%s Volume %d %s %s", 
                op_string[vol->op], 
                obj->ID, 
                verbose ? get_dims_string(obj, buf) : "",
                verbose ? xform_string(vol->xform, buf2, 64) : ""
                );
        break;

    case OBJ_GROUP:
        grp = (Group *)obj;
        if (grp->title[0] == '\0' || !verbose)
            sprintf_s(descr, descr_len, "%s Group %d %s", 
                    op_string[grp->op], 
                    obj->ID,
                    verbose ? xform_string(grp->xform, buf, 64) : ""
                    );
        else
            sprintf_s(descr, descr_len, "%s Group %d: %s %s", 
                    op_string[grp->op], 
                    obj->ID, 
                    grp->title,
                    xform_string(grp->xform, buf, 64)
            );
        break;
    }

    return descr;
}

// Populate a treeview item for the components of an object.
void
populate_treeview_object(Object *obj, Object *parent, HTREEITEM hItem, char *tag)
{
    TVINSERTSTRUCT tvins;
    TVITEM tvi;
    char descr[128];
    char tagged_descr[128];
    int i;
    Face *face;
    Edge *edge;
    ArcEdge* ae;
    BezierEdge* be;
    Object *o;

    switch (obj->type)
    {
    case OBJ_POINT:
        obj_description(obj, descr, 128, TRUE);
        if (tag != NULL)
        {
            strcpy_s(tagged_descr, 128, tag);
            strcat_s(tagged_descr, 128, " ");
            strcat_s(tagged_descr, 128, descr);
            tvi.pszText = tagged_descr;
        }
        else
        {
            tvi.pszText = descr;
        }
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
        tvins.hInsertAfter = TVI_LAST;
        hItem = (HTREEITEM)SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvins);
        break;

    case OBJ_EDGE:
        edge = (Edge *)obj;
        tvi.pszText = obj_description(obj, descr, 128, TRUE);
        tvi.cchTextMax = strlen(tvi.pszText);
        tvi.lParam = (LPARAM)obj;
        tvi.mask = TVIF_TEXT | TVIF_PARAM;
        tvi.state = 0;
        tvi.stateMask = 0;
        if (is_selected_direct(obj, &o))
        {
            tvi.mask |= TVIF_STATE;
            tvi.state |= TVIS_BOLD;
            tvi.stateMask |= TVIS_BOLD;
        }
        if (obj->tv_flags & TVIS_EXPANDED)
        {
            tvi.mask |= TVIF_STATE;
            tvi.state |= TVIS_EXPANDED;
            tvi.stateMask |= TVIS_EXPANDED;
        }
        tvins.item = tvi;
        tvins.hParent = hItem;
        tvins.hInsertAfter = TVI_LAST;
        hItem = (HTREEITEM)SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvins);
        if (parent->lock < LOCK_POINTS)
        {
            populate_treeview_object((Object *)edge->endpoints[0], parent, hItem, "[0]");
            if (edge->type == EDGE_ARC)
            {
                ae = (ArcEdge *)edge;
                populate_treeview_object((Object *)ae->centre, parent, hItem, "C");
            }
            else if (edge->type == EDGE_BEZIER)
            {
                be = (BezierEdge *)edge;
                populate_treeview_object((Object *)be->ctrlpoints[0], parent, hItem, "C0");
                populate_treeview_object((Object *)be->ctrlpoints[1], parent, hItem, "C1");
            }
            populate_treeview_object((Object*)edge->endpoints[1], parent, hItem, "[1]");
        }
        break;

    case OBJ_FACE:
        face = (Face *)obj;
        tvi.pszText = obj_description(obj, descr, 128, TRUE);
        tvi.cchTextMax = strlen(tvi.pszText);
        tvi.lParam = (LPARAM)obj;
        tvi.mask = TVIF_TEXT | TVIF_PARAM;
        tvi.state = 0;
        tvi.stateMask = 0;
        if (is_selected_direct(obj, &o))
        {
            tvi.mask |= TVIF_STATE;
            tvi.state |= TVIS_BOLD;
            tvi.stateMask |= TVIS_BOLD;
        }
        if (obj->tv_flags & TVIS_EXPANDED)
        {
            tvi.mask |= TVIF_STATE;
            tvi.state |= TVIS_EXPANDED;
            tvi.stateMask |= TVIS_EXPANDED;
        }
        tvins.item = tvi;
        tvins.hParent = hItem;
        tvins.hInsertAfter = TVI_LAST;
        hItem = (HTREEITEM)SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvins);
        if (parent->lock < LOCK_EDGES)
        {
            for (i = 0; i < face->n_edges; i++)
                populate_treeview_object((Object *)face->edges[i], parent, hItem, NULL);
        }
        break;

    case OBJ_VOLUME:
        tvi.pszText = obj_description(obj, descr, 128, TRUE);
        tvi.cchTextMax = strlen(tvi.pszText);
        tvi.lParam = (LPARAM)obj;
        tvi.mask = TVIF_TEXT | TVIF_PARAM;
        tvi.state = 0;
        tvi.stateMask = 0;
        if (is_selected_direct(obj, &o))
        {
            tvi.mask |= TVIF_STATE;
            tvi.state |= TVIS_BOLD;
            tvi.stateMask |= TVIS_BOLD;
        }
        if (obj->tv_flags & TVIS_EXPANDED)
        {
            tvi.mask |= TVIF_STATE;
            tvi.state |= TVIS_EXPANDED;
            tvi.stateMask |= TVIS_EXPANDED;
        }
        tvins.item = tvi;
        tvins.hParent = hItem;
        tvins.hInsertAfter = TVI_LAST;
        hItem = (HTREEITEM)SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvins);
        if (parent->lock < LOCK_FACES)
        {
            for (i = 0, face = (Face *)((Volume *)obj)->faces.head; face != NULL; face = (Face *)face->hdr.next, i++)
            {
                if (i < TREEVIEW_LIMIT)
                {
                    populate_treeview_object((Object *)face, parent, hItem, NULL);
                }
                else
                {
                    tvi.pszText = "(Limit on faces reached)";
                    tvi.cchTextMax = strlen(tvi.pszText);
                    tvi.lParam = (LPARAM)NULL;
                    tvi.mask = TVIF_TEXT;
                    tvins.item = tvi;
                    tvins.hParent = hItem;
                    tvins.hInsertAfter = TVI_LAST;
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
            tvi.pszText = obj_description(obj, descr, 128, TRUE);
            tvi.cchTextMax = strlen(tvi.pszText);
            tvi.lParam = (LPARAM)obj;
            tvi.mask = TVIF_TEXT | TVIF_PARAM;
            tvi.state = 0;
            tvi.stateMask = 0;
            if (is_selected_direct(obj, &o))
            {
                tvi.mask |= TVIF_STATE;
                tvi.state |= TVIS_BOLD;
                tvi.stateMask |= TVIS_BOLD;
            }
            if (obj->tv_flags & TVIS_EXPANDED)
            {
                tvi.mask |= TVIF_STATE;
                tvi.state |= TVIS_EXPANDED;
                tvi.stateMask |= TVIS_EXPANDED;
            }
            tvins.item = tvi;
            tvins.hParent = hItem;
            tvins.hInsertAfter = TVI_LAST;
            hGroup = (HTREEITEM)SendDlgItemMessage(hWndTree, IDC_TREEVIEW, TVM_INSERTITEM, 0, (LPARAM)&tvins);
            populate_treeview_tree((Group *)obj, hGroup);
        }
        else
        {
            populate_treeview_object(obj, obj, hItem, NULL);
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
    TVITEM* tvi;
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
            obj = treeview_highlight;
            treeview_highlight = NULL;      // in case it goes away in the context menu operation
            contextmenu(obj, pt);
            break;

        case TVN_ITEMEXPANDED:              // set or clear the expanded state flag in the obj
            nmtv = (NMTREEVIEW*)lParam;
            tvi = &nmtv->itemNew;
            ((Object*)tvi->lParam)->tv_flags = tvi->state & TVIS_EXPANDED;
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

