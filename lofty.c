#include "lofty.h"
#include "trackbal.h"
#include "resource.h"

char	filename[256];		/* Input filename */
char    title[128];
int	np = 0;			/* counter of points */
int	ns = 0;			/* counter of sections */
Point3D	*list = NULL;		/* cross section list head */
Point3D *current;		/* currently displayed section */
Point3D *currve;		/* currently displayed curve on plan/elev */
double  default_xmax = 0.4;
double  default_ymin = 0.0;
double  default_ymax = 0.5;     /* default bounds */
double	xmin, ymin, zmin;	/* bounds */
double	xmax, ymax, zmax;
double  tension_xy = 0.3;       /* default tensions */
double  tension_z = 0.3;
int	view3D;			/* Viewing 3D? */
int     view_plan = 0;          /* Viewing plan? */
int     view_elev = 0;          /* Viewing elevation? */
int	view_devel = 0;		/* Viewing development? */
int	results_view = 0;	/* Viewing solution results? */
int	lock_bgnd = 1;
int	marg = 30;		/* Margin in pixels (screen only) */
int     printing = 0;           /* Are we printing? */
double	print_x_size = 0.210;
double	print_y_size = 0.297;	/* Printer size */
HANDLE  print_dm;
HANDLE  print_dn;

HINSTANCE    hInst;
HWND    hWndMain;
HWND	hWndSolve;

int     show_unselected = 1;
int     show_background = 1;
int     show_contours_xy = 0;
int     show_contours_zx = 0;
double	contour_start_xy = -2.0;
double	contour_end_xy = 0.0;
double	contour_inc_xy = 0.5;
double	contour_start_zx = 0.0;
double	contour_end_zx = 1.0;
double	contour_inc_zx = 0.2;
double  contour_inclin_zx = 0;
int	show_zlines = 0;
int	zline = 0;
char	echo1[128] = "\0";
char	echo2[128] = "\0";
char	echo3[128] = "\0";
double	print_scale = 5.0;	/* Scale when printing */
double	background_image_scale = 5.0;  /* Scale of background images */
int	fit_to_page = 0;	/* Fit to page when printing */
double  grid_step = 0.02;       /* Grid step when printing */

Elt     *e;                     /* Array of elements */
int     nels;
Node3D	*node;			/* Array of node points */
int	nnode;

int	*line_start;
int	n_lines;
int     solved = FALSE;

int     show_elements = 1;      /* Show various things */
int     show_normals = 0;
int     show_velocity_vec = 0;
int     show_velocity_mag = 1;
int     show_velocity_norm = 0;
int     show_what = 0;
int     show_pressures = 0;
int	show_zones = 1;
int	print_preview = 0;
int     printing_3d = 0;

Vec3D   vinf = {0, 0, -15};     /* 15 m/s (about 50 km/h) from the +ve Z direction */
int	use_turb = 1;		/* enable all the turb/sep stuff */
int     use_ground = 0;         /* take ground into account */
double  ground_y = 0.0;
int     use_skin = 1;           /* take skin drag into account */

Wheel	wheels[4] = {0, };

double	bulk_density = 1250;	/* density of bulk material (1.25 g/cm3 for PLA) */
double	infill_percentage = 10;	/* percentage density for infill */
double	skin_thickness = 2.0;	/* thickness of skin in mm (perimeter) */


#define DENSITY_AIR	1.293
#define VISCOSITY_AIR	1.78e-5
#define DENSITY_WATER	1000.0
#define VISCOSITY_WATER	0.001


double  density = DENSITY_AIR;       /* density of air */
double  viscosity = VISCOSITY_AIR;   /* viscosity (mu) of air */
double  reynolds_turb = 3.5e5;	/* Re for threhsold of turbulent boundary layer */
double  press_slope_lam = 1.3; /* Cp/Z threshold for separation under laminar BL */
double  press_slope_turb = 8.5; /* Cp/Z threshold for separation under turbulent BL */

double	cd;
double	force;
double	power;
double  vel_min;
double  vel_max;
double  press_min;
double  press_max;
double  norm_min;
double  norm_max;
Vec3D   total_force;
double  frontal_area;
double  surface_area;
double	skin_mass;
double	infill_mass;
double	total_mass;


/*
 * Geometry helpers.
 */
void
normal
(
    double  x1,
    double  y1,
    double  z1,
    double  x2,
    double  y2,
    double  z2,
    double  x3,
    double  y3,
    double  z3,
    double  v[3]
)
{
    double  mag;

    v[0] = y1*(z2-z3) + y2*(z3-z1) + y3*(z1-z2);
    v[1] = z1*(x2-x3) + z2*(x3-x1) + z3*(x1-x2);
    v[2] = x1*(y2-y3) + x2*(y3-y1) + x3*(y1-y2);

    mag = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    v[0] /= mag;
    v[1] /= mag;
    v[2] /= mag;
}


void
centroid
(
    Node3D	a,
    Node3D	b,
    Node3D	c,
    Node3D	d,
    Coord3D	*o
)
{
    double      m1[3], m2[3];

    m1[0] = (a.x + c.x) / 2;
    m1[1] = (a.y + c.y) / 2;
    m1[2] = (a.z + c.z) / 2;
    m2[0] = (b.x + d.x) / 2;
    m2[1] = (b.y + d.y) / 2;
    m2[2] = (b.z + d.z) / 2;
    o->x = (m1[0] + m2[0]) / 2;
    o->y = (m1[1] + m2[1]) / 2;
    o->z = (m1[2] + m2[2]) / 2;
}



/*
 * Throw away everything.
 */
static void
clobber(void)
{
    Point3D	*c;
    Point3D	*cp;
    Point3D	*nextc;
    Point3D	*nextcp;

    for (c = list; c != NULL; c = nextc)
    {
	nextc = c->next_in_section;

	for (cp = c; cp != NULL; cp = nextcp)
	{
	    if (c == list)
	    {
		if (cp->background_file != NULL)
		    free(cp->background_file);
		if (cp->sect_name != NULL)
		    free(cp->sect_name);
	    }
	    	    
	    nextcp = cp->next;
	    free(cp);
	}
    }

    if (e != NULL)
	free(e);
    nels = 0;
    e = NULL;

    if (node != NULL)
	free(node);
    nnode = 0;
    node = NULL;

    np = 0;
    filename[0] = '\0';
    list = NULL;
    current = NULL;
    currve = NULL;
}


/*
 * Save data points to file.
 */
static void
save_file(void)
{
    Point3D *c;
    Point3D *cs;
    FILE    *output;
    int	    i;
    Wheel   *w;

    output = fopen(filename, "w");
    fprintf(output, "%s\n", title);

    for (cs = list; cs != NULL; cs = cs->next_section_head)
    {
	fprintf(output, "s %s\n", cs->sect_name);
	if (cs->background != NULL)
	    fprintf(output, "b %lf %lf %lf %s\n", cs->bx, cs->by, cs->scale, cs->background_file);
	for (c = cs; c != NULL; c = c->next_in_section)
	{
	    fprintf
	    (
		output, 
		"p %lf %lf %lf %d %d %d %d %lf %lf\n", 
		c->x, c->y, c->z, 
		c->cusp, c->cusp_curve, c->endpt, c->hard,
		c->tension_xy, c->tension_z
	    );
	    fprintf(output, "t %lf\n", c->t);
	    for (i = 0; i < 4; i++)
		fprintf
		(
		    output, 
		    "c %lf %lf %lf %d\n",
		    c->ctrl[i].x, c->ctrl[i].y, c->ctrl[i].z, 
		    c->ctrl[i].user_moved
		);
	}
    }

    for (i = 0; i < 4; i++)
    {
	w = &wheels[i];
	fprintf
	(
	    output, 
	    "w %lf %lf %lf %lf %lf %lf\n", 
	    w->centre.x, w->centre.y, w->centre.z, 
	    w->radius, w->theta_xz, w->theta_xy
	);
    }
    fclose(output);

    save_elements();
}


/*
 * Check if something needs to be saved before closing.
 */
static BOOL
check_before_closing(BOOL modified, HWND hWnd)
{
    if (modified)
    {
	switch (MessageBox(hWnd, "File modified. Save changes?", filename, MB_YESNOCANCEL))
	{
	case IDCANCEL:
	    return FALSE;
	case IDYES:
            if (filename[0] == '\0')
            {
                OPENFILENAME    ofn;
                char            file_title[64];

                memset(&ofn, 0, sizeof(OPENFILENAME));
	        ofn.lStructSize = sizeof(OPENFILENAME);
	        ofn.hwndOwner = hWnd;
	        ofn.lpstrFilter = "Sections (*.lft)\0*.lft\0All Files (*.*)\0*.*\0";
	        ofn.lpstrTitle = "Save a Section File";
	        ofn.lpstrFile = filename;
	        ofn.nMaxFile = sizeof(filename);
	        ofn.lpstrFileTitle = file_title;
	        ofn.nMaxFileTitle = sizeof(file_title);
	        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
	        ofn.lpstrDefExt = "lft";

	        if (!GetSaveFileName(&ofn))
		    break;
            }
	    save_file();
	    /* fallthrough */
	case IDNO:
	    break;
	}
    }

    clobber();
    InvalidateRect(hWnd, NULL, TRUE);

    return TRUE;
}


/*
 * Deep copy list to undo stack.
 */
void
checkpoint(void)
{
    // TODO: Implement some sort of Undo stack. Low priority.
}



/*
 * About dialog. A completely silly use of version functions.
 */
int WINAPI
about_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DWORD   dummy;
    int     size, bytes;
    char    *buf;
    char    *edit;
    char    *edit2;
    PAINTSTRUCT ps;
    HDC     hdc;

    switch (msg)
    {
    case WM_INITDIALOG:
        size = GetFileVersionInfoSize("lofty.exe", &dummy);
        buf = malloc(size);
        GetFileVersionInfo("lofty.exe", 0, size, buf);
        VerQueryValue(buf, "\\StringFileInfo\\0c0904b0\\InternalName", &edit, &bytes);
        SetDlgItemText(hWnd, IDC_ABOUT_PRODUCT_NAME, edit);
        VerQueryValue(buf, "\\StringFileInfo\\0c0904b0\\ProductVersion", &edit, &bytes);
        SetDlgItemText(hWnd, IDC_ABOUT_PRODUCT_VERSION, edit);
        VerQueryValue(buf, "\\StringFileInfo\\0c0904b0\\SpecialBuild", &edit, &bytes);
        SetDlgItemText(hWnd, IDC_ABOUT_PRODUCT_BUILD, edit);
        VerQueryValue(buf, "\\StringFileInfo\\0c0904b0\\LegalCopyright", &edit, &bytes);
        VerQueryValue(buf, "\\StringFileInfo\\0c0904b0\\CompanyName", &edit2, &bytes);
        strcat(edit, edit2);
        SetDlgItemText(hWnd, IDC_ABOUT_COY_COPYRIGHT, edit);
        free(buf);

        break;

    case WM_PAINT:
	hdc = BeginPaint(hWnd, &ps);
        DrawIcon(hdc, 5, 5, LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1)));
        EndPaint(hWnd, &ps);

        break;

    case WM_COMMAND:
	switch (LOWORD(wParam))
	{
	case IDOK:
	case IDCANCEL:
	    EndDialog(hWnd, 1);
	    return 1;
	}

	break;
    }

    return 0;
}



/*
 * Point properties dialog.
 */
int WINAPI
point_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static Point3D  *cs;
    char	    edit[128];
    int		    hard;

    switch (msg)
    {
    case WM_INITDIALOG:
	cs = (Point3D *)lParam;

	sprintf(edit, "%f", cs->x);
	SetDlgItemText(hWnd, IDC_POINT_X, edit);
	sprintf(edit, "%f", cs->y);
	SetDlgItemText(hWnd, IDC_POINT_Y, edit);
	sprintf(edit, "%f", cs->z);
	SetDlgItemText(hWnd, IDC_POINT_Z, edit);

	SendDlgItemMessage(hWnd, IDC_CUSP, BM_SETCHECK, cs->cusp ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hWnd, IDC_CUSP_CURVE, BM_SETCHECK, cs->cusp_curve ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hWnd, IDC_ENDPT, BM_SETCHECK, cs->endpt ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hWnd, IDC_HARD_PT, BM_SETCHECK, cs->hard ? BST_CHECKED : BST_UNCHECKED, 0);

	EnableWindow(GetDlgItem(hWnd, IDC_POINT_X), !cs->hard);
	EnableWindow(GetDlgItem(hWnd, IDC_POINT_Y), !cs->hard);
	EnableWindow(GetDlgItem(hWnd, IDC_POINT_Z), !cs->hard);
        break;

    case WM_COMMAND:
	switch (LOWORD(wParam))
	{
	case IDOK:
	    GetDlgItemText(hWnd, IDC_POINT_X, edit, 128);
	    cs->x = atof(edit);
	    GetDlgItemText(hWnd, IDC_POINT_Y, edit, 128);
	    cs->y = atof(edit);
	    GetDlgItemText(hWnd, IDC_POINT_Z, edit, 128);
	    cs->z = atof(edit);

	    cs->cusp = SendDlgItemMessage(hWnd, IDC_CUSP, BM_GETSTATE, 0, 0) == BST_CHECKED;
	    cs->cusp_curve = SendDlgItemMessage(hWnd, IDC_CUSP_CURVE, BM_GETSTATE, 0, 0) == BST_CHECKED;
	    cs->endpt = SendDlgItemMessage(hWnd, IDC_ENDPT, BM_GETSTATE, 0, 0) == BST_CHECKED;
	    cs->hard = SendDlgItemMessage(hWnd, IDC_HARD_PT, BM_GETSTATE, 0, 0) == BST_CHECKED;

	    EndDialog(hWnd, 1);
	    return 1;

	case IDCANCEL:
	    EndDialog(hWnd, 0);
	    return 1;

	case IDC_HARD_PT:
	    hard = SendDlgItemMessage(hWnd, IDC_HARD_PT, BM_GETSTATE, 0, 0) == BST_CHECKED;   // TODO - unreliable?
	    EnableWindow(GetDlgItem(hWnd, IDC_POINT_X), !hard);
	    EnableWindow(GetDlgItem(hWnd, IDC_POINT_Y), !hard);
	    EnableWindow(GetDlgItem(hWnd, IDC_POINT_Z), !hard);
	    break;
	}

	break;
    }

    return 0;
}



/*
 * Section properties dialog.
 */
int WINAPI
section_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static Point3D  *cs;
    char	    edit[128];
    double	    z, txy, tz;
    int             nose;

    switch (msg)
    {
    case WM_INITDIALOG:
	cs = (Point3D *)lParam;

	SetDlgItemText(hWnd, IDC_SECTION_NAME, cs->sect_name);
	sprintf(edit, "%f", cs->z);
	SetDlgItemText(hWnd, IDC_SECTION_Z, edit);
	sprintf(edit, "%f", cs->tension_xy);
	SetDlgItemText(hWnd, IDC_SECTION_TXY, edit);
	sprintf(edit, "%f", cs->tension_z);
	SetDlgItemText(hWnd, IDC_SECTION_TZ, edit);

        nose = FALSE;
        if (cs->next_section_head == NULL)
        {
            Point3D *c = cs;

            nose = TRUE;
            while (c != NULL)
            {
                if (!c->endpt || !c->midline)
                {
                    nose = FALSE;
                    break;
                }
		c = c->next_in_section;
	    }
        }

        SendDlgItemMessage(hWnd, IDC_SECTION_NOSE, BM_SETCHECK, nose ? BST_CHECKED : BST_UNCHECKED, 0);

	break;

    case WM_COMMAND:
	switch (LOWORD(wParam))
	{
	case IDOK:
	    GetDlgItemText(hWnd, IDC_SECTION_NAME, cs->sect_name, 128);
	    GetDlgItemText(hWnd, IDC_SECTION_Z, edit, 128);
	    z = atof(edit);
	    GetDlgItemText(hWnd, IDC_SECTION_TXY, edit, 128);
	    txy = atof(edit);
	    GetDlgItemText(hWnd, IDC_SECTION_TZ, edit, 128);
	    tz = atof(edit);

            /*
             * DO NOT allow a blank section name. It can't be read in properly
             * by C's stupid I/O routines...
             */
            if (cs->sect_name[0] == '\0')
                sprintf(cs->sect_name, "Z=%f", z);

            /*
             * Set the nose up.
             */
            nose = SendDlgItemMessage(hWnd, IDC_SECTION_NOSE, BM_GETSTATE, 0, 0) == BST_CHECKED;
            while (cs != NULL)
	    {
		cs->z = z;
		cs->tension_xy = txy;
		cs->tension_z = tz;
                if (nose)
                {
                    cs->x = 0;
                    cs->midline = TRUE;
                    cs->endpt = TRUE;
                }

		cs = cs->next_in_section;
	    }

	    EndDialog(hWnd, 1);
	    return 1;

	case IDCANCEL:
	    EndDialog(hWnd, 0);
	    return 1;
	}

	break;
    }

    return 0;
}


/*
 * Solution properties dialog. This is a modeless dialog.
 */
int WINAPI
solve_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static Point3D  *cs;
    char	    edit[128];
    Vec3D	    v;
    double	    v0, a;
    int             new_ground;
    double          new_ground_y;

    switch (msg)
    {
    case WM_INITDIALOG:
	sprintf(edit, "%f", density);
	SetDlgItemText(hWnd, IDC_DENSITY, edit);
	sprintf(edit, "%f", viscosity);
	SetDlgItemText(hWnd, IDC_VISCOSITY, edit);

	SendDlgItemMessage(hWnd, IDC_ENABLE_TURB, BM_SETCHECK, use_turb ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hWnd, IDC_ENABLE_GROUND, BM_SETCHECK, use_ground ? BST_CHECKED : BST_UNCHECKED, 0);
	sprintf(edit, "%f", ground_y);
	SetDlgItemText(hWnd, IDC_GROUND_Y, edit);
	SendDlgItemMessage(hWnd, IDC_ENABLE_SKIN, BM_SETCHECK, use_skin ? BST_CHECKED : BST_UNCHECKED, 0);

	sprintf(edit, "%f", reynolds_turb);
	SetDlgItemText(hWnd, IDC_TURB_RE, edit);
	sprintf(edit, "%f", press_slope_lam);
	SetDlgItemText(hWnd, IDC_LAM_CPZ_SLOPE, edit);
	sprintf(edit, "%f", press_slope_turb);
	SetDlgItemText(hWnd, IDC_TURB_CPZ_SLOPE, edit);

	sprintf(edit, "%f", vinf.u);
	SetDlgItemText(hWnd, IDC_VINF_X, edit);
	sprintf(edit, "%f", vinf.w);
	SetDlgItemText(hWnd, IDC_VINF_Z, edit);

	sprintf(edit, "%f", dist(vinf));
	SetDlgItemText(hWnd, IDC_VINF, edit);
	sprintf(edit, "%f", 57.29577 * atan2(vinf.u, -vinf.w));
	SetDlgItemText(hWnd, IDC_VINF_ANGLE, edit);
	
	break;
	
    case WM_COMMAND:
	switch (LOWORD(wParam))
	{
	case IDOK:
	case IDC_SOLVE_NOW:
	    GetDlgItemText(hWnd, IDC_DENSITY, edit, 128);
	    density = atof(edit);
	    GetDlgItemText(hWnd, IDC_VISCOSITY, edit, 128);
	    viscosity = atof(edit);

	    use_turb = SendDlgItemMessage(hWnd, IDC_ENABLE_TURB, BM_GETSTATE, 0, 0) == BST_CHECKED;
	    new_ground = SendDlgItemMessage(hWnd, IDC_ENABLE_GROUND, BM_GETSTATE, 0, 0) == BST_CHECKED;
            if (new_ground != use_ground)
                solved = FALSE;
            use_ground = new_ground;

	    GetDlgItemText(hWnd, IDC_GROUND_Y, edit, 128);
	    new_ground_y = atof(edit);
            if (new_ground_y != ground_y)
                solved = FALSE;
            ground_y = new_ground_y;

      	    use_skin = SendDlgItemMessage(hWnd, IDC_ENABLE_SKIN, BM_GETSTATE, 0, 0) == BST_CHECKED;

	    GetDlgItemText(hWnd, IDC_TURB_RE, edit, 128);
	    reynolds_turb = atof(edit);
	    GetDlgItemText(hWnd, IDC_LAM_CPZ_SLOPE, edit, 128);
	    press_slope_lam = atof(edit);
	    GetDlgItemText(hWnd, IDC_TURB_CPZ_SLOPE, edit, 128);
	    press_slope_turb = atof(edit);

	    GetDlgItemText(hWnd, IDC_VINF_X, edit, 128);
	    vinf.u = atof(edit);
	    GetDlgItemText(hWnd, IDC_VINF_Z, edit, 128);
	    vinf.w = atof(edit);

	    if (LOWORD(wParam) == IDOK)
	    {
		DestroyWindow(hWnd);
		return 1;
	    }
	    SendMessage(hWndMain, WM_COMMAND, IDC_SOLVE_AGAIN, 0);
	    break;

	case IDCANCEL:
	    DestroyWindow(hWnd);
	    return 1;

	case IDC_VINF_X:
	case IDC_VINF_Z:
	    if (HIWORD(wParam) == EN_KILLFOCUS)
	    {
		GetDlgItemText(hWnd, IDC_VINF_X, edit, 128);
		v.u = atof(edit);
		v.v = 0;
		GetDlgItemText(hWnd, IDC_VINF_Z, edit, 128);
		v.w = atof(edit);
		sprintf(edit, "%f", dist(v));
		SetDlgItemText(hWnd, IDC_VINF, edit);
		sprintf(edit, "%f", 57.29577 * atan2(v.u, -v.w));
		SetDlgItemText(hWnd, IDC_VINF_ANGLE, edit);
	    }
	    break;

	case IDC_VINF:
	case IDC_VINF_ANGLE:
	    if (HIWORD(wParam) == EN_KILLFOCUS)
	    {
		GetDlgItemText(hWnd, IDC_VINF, edit, 128);
		v0 = atof(edit);
		GetDlgItemText(hWnd, IDC_VINF_ANGLE, edit, 128);
		a = atof(edit) / 57.29577;
		v.u =  v0 * sin(a);
		v.w = -v0 * cos(a);
		sprintf(edit, "%f", v.u);
		SetDlgItemText(hWnd, IDC_VINF_X, edit);
		sprintf(edit, "%f", v.w);
    		SetDlgItemText(hWnd, IDC_VINF_Z, edit);
	    }
	    break;

	case IDC_DEFAULTS_AIR:
	    sprintf(edit, "%f", DENSITY_AIR);
	    SetDlgItemText(hWnd, IDC_DENSITY, edit);
	    sprintf(edit, "%f", VISCOSITY_AIR);
	    SetDlgItemText(hWnd, IDC_VISCOSITY, edit);
	    break;

	case IDC_DEFAULTS_WATER:
	    sprintf(edit, "%f", DENSITY_WATER);
	    SetDlgItemText(hWnd, IDC_DENSITY, edit);
	    sprintf(edit, "%f", VISCOSITY_WATER);
	    SetDlgItemText(hWnd, IDC_VISCOSITY, edit);
	    break;
	}

	break;
    }

    return 0;
}



/*
 * Preferences dialog.
 */
int WINAPI
pref_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    char	    edit[128];

    switch (msg)
    {
    case WM_INITDIALOG:
	SetDlgItemText(hWnd, IDC_TITLE, title);

	sprintf(edit, "%f", xmax);
	SetDlgItemText(hWnd, IDC_PREF_XMAX, edit);
	sprintf(edit, "%f", ymin);
	SetDlgItemText(hWnd, IDC_PREF_YMIN, edit);
	sprintf(edit, "%f", ymax);
	SetDlgItemText(hWnd, IDC_PREF_YMAX, edit);

	sprintf(edit, "%f", bulk_density);
	SetDlgItemText(hWnd, IDC_PREF_BULK_DENSITY, edit);
	sprintf(edit, "%f", infill_percentage);
	SetDlgItemText(hWnd, IDC_PREF_INFILL_PERCENTAGE, edit);
	sprintf(edit, "%f", skin_thickness);
	SetDlgItemText(hWnd, IDC_PREF_SKIN_THICKNESS, edit);

	sprintf(edit, "%f", tension_xy);
	SetDlgItemText(hWnd, IDC_PREF_TENSION_XY, edit);
	sprintf(edit, "%f", tension_z);
	SetDlgItemText(hWnd, IDC_PREF_TENSION_Z, edit);
	sprintf(edit, "%f", background_image_scale);
	SetDlgItemText(hWnd, IDC_PREF_BK_IMAGE_SCALE, edit);

	sprintf(edit, "%f", print_scale);
	SetDlgItemText(hWnd, IDC_PREF_SCALE, edit);
	SendDlgItemMessage(hWnd, IDC_PREF_FIT, BM_SETCHECK, fit_to_page ? BST_CHECKED : BST_UNCHECKED, 0);
	sprintf(edit, "%f", grid_step);
	SetDlgItemText(hWnd, IDC_PREF_GRID_STEP, edit);
	
        SendDlgItemMessage(hWnd, IDC_PREF_CONTOURS_XY, BM_SETCHECK, show_contours_xy ? BST_CHECKED : BST_UNCHECKED, 0);
	sprintf(edit, "%f", contour_start_xy);
	SetDlgItemText(hWnd, IDC_PREF_CONT_START_XY, edit);
	sprintf(edit, "%f", contour_end_xy);
	SetDlgItemText(hWnd, IDC_PREF_CONT_END_XY, edit);
	sprintf(edit, "%f", contour_inc_xy);
	SetDlgItemText(hWnd, IDC_PREF_CONT_INC_XY, edit);

        SendDlgItemMessage(hWnd, IDC_PREF_CONTOURS_ZX, BM_SETCHECK, show_contours_zx ? BST_CHECKED : BST_UNCHECKED, 0);
	sprintf(edit, "%f", contour_start_zx);
	SetDlgItemText(hWnd, IDC_PREF_CONT_START_ZX, edit);
	sprintf(edit, "%f", contour_end_zx);
	SetDlgItemText(hWnd, IDC_PREF_CONT_END_ZX, edit);
	sprintf(edit, "%f", contour_inc_zx);
	SetDlgItemText(hWnd, IDC_PREF_CONT_INC_ZX, edit);
	sprintf(edit, "%f", contour_inclin_zx);
	SetDlgItemText(hWnd, IDC_PREF_CONT_INCLIN_ZX, edit);

        break;

    case WM_COMMAND:
	switch (LOWORD(wParam))
	{
	case IDOK:
	    GetDlgItemText(hWnd, IDC_TITLE, title, 128);

	    GetDlgItemText(hWnd, IDC_PREF_XMAX, edit, 128);
	    xmax = atof(edit);
	    GetDlgItemText(hWnd, IDC_PREF_YMIN, edit, 128);
	    ymin = atof(edit);
	    GetDlgItemText(hWnd, IDC_PREF_YMAX, edit, 128);
	    ymax = atof(edit);

	    GetDlgItemText(hWnd, IDC_PREF_BULK_DENSITY, edit, 128);
	    bulk_density = atof(edit);
	    GetDlgItemText(hWnd, IDC_PREF_INFILL_PERCENTAGE, edit, 128);
	    infill_percentage = atof(edit);
	    GetDlgItemText(hWnd, IDC_PREF_SKIN_THICKNESS, edit, 128);
	    skin_thickness = atof(edit);

            GetDlgItemText(hWnd, IDC_PREF_TENSION_XY, edit, 128);
	    tension_xy = atof(edit);
	    GetDlgItemText(hWnd, IDC_PREF_TENSION_Z, edit, 128);
	    tension_z = atof(edit);
	    GetDlgItemText(hWnd, IDC_PREF_BK_IMAGE_SCALE, edit, 128);
	    background_image_scale = atof(edit);

	    GetDlgItemText(hWnd, IDC_PREF_SCALE, edit, 128);
	    print_scale = atof(edit);
	    fit_to_page = SendDlgItemMessage(hWnd, IDC_PREF_FIT, BM_GETSTATE, 0, 0) == BST_CHECKED;
	    GetDlgItemText(hWnd, IDC_PREF_GRID_STEP, edit, 128);
	    grid_step = atof(edit);

	    show_contours_xy = SendDlgItemMessage(hWnd, IDC_PREF_CONTOURS_XY, BM_GETSTATE, 0, 0) == BST_CHECKED;
            GetDlgItemText(hWnd, IDC_PREF_CONT_START_XY, edit, 128);
	    contour_start_xy = atof(edit);
	    GetDlgItemText(hWnd, IDC_PREF_CONT_END_XY, edit, 128);
	    contour_end_xy = atof(edit);
	    GetDlgItemText(hWnd, IDC_PREF_CONT_INC_XY, edit, 128);
	    contour_inc_xy = atof(edit);
	    
	    show_contours_zx = SendDlgItemMessage(hWnd, IDC_PREF_CONTOURS_ZX, BM_GETSTATE, 0, 0) == BST_CHECKED;
            GetDlgItemText(hWnd, IDC_PREF_CONT_START_ZX, edit, 128);
	    contour_start_zx = atof(edit);
	    GetDlgItemText(hWnd, IDC_PREF_CONT_END_ZX, edit, 128);
	    contour_end_zx = atof(edit);
	    GetDlgItemText(hWnd, IDC_PREF_CONT_INC_ZX, edit, 128);
	    contour_inc_zx = atof(edit);
	    GetDlgItemText(hWnd, IDC_PREF_CONT_INCLIN_ZX, edit, 128);
	    contour_inclin_zx = atof(edit);
            
            EndDialog(hWnd, 1);
	    return 1;

	case IDCANCEL:
	    EndDialog(hWnd, 0);
	    return 1;
	}

	break;
    }

    return 0;
}



/*
 * Wheels dialog.
 */
int WINAPI
wheel_dialog(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    char	    edit[128];

    switch (msg)
    {
    case WM_INITDIALOG:
	sprintf(edit, "%f", wheels[0].centre.x);
	SetDlgItemText(hWnd, IDC_WHEEL_X1, edit);
	sprintf(edit, "%f", wheels[0].centre.y);
	SetDlgItemText(hWnd, IDC_WHEEL_Y1, edit);
	sprintf(edit, "%f", wheels[0].centre.z);
	SetDlgItemText(hWnd, IDC_WHEEL_Z1, edit);
	sprintf(edit, "%f", wheels[0].radius);
	SetDlgItemText(hWnd, IDC_WHEEL_R1, edit);
	sprintf(edit, "%f", wheels[0].theta_xz);
	SetDlgItemText(hWnd, IDC_WHEEL_THETA_XZ1, edit);
	sprintf(edit, "%f", wheels[0].theta_xy);
	SetDlgItemText(hWnd, IDC_WHEEL_THETA_XY1, edit);

	sprintf(edit, "%f", wheels[1].centre.x);
	SetDlgItemText(hWnd, IDC_WHEEL_X2, edit);
	sprintf(edit, "%f", wheels[1].centre.y);
	SetDlgItemText(hWnd, IDC_WHEEL_Y2, edit);
	sprintf(edit, "%f", wheels[1].centre.z);
	SetDlgItemText(hWnd, IDC_WHEEL_Z2, edit);
	sprintf(edit, "%f", wheels[1].radius);
	SetDlgItemText(hWnd, IDC_WHEEL_R2, edit);
	sprintf(edit, "%f", wheels[1].theta_xz);
	SetDlgItemText(hWnd, IDC_WHEEL_THETA_XZ2, edit);
	sprintf(edit, "%f", wheels[1].theta_xy);
	SetDlgItemText(hWnd, IDC_WHEEL_THETA_XY2, edit);

	sprintf(edit, "%f", wheels[2].centre.x);
	SetDlgItemText(hWnd, IDC_WHEEL_X3, edit);
	sprintf(edit, "%f", wheels[2].centre.y);
	SetDlgItemText(hWnd, IDC_WHEEL_Y3, edit);
	sprintf(edit, "%f", wheels[2].centre.z);
	SetDlgItemText(hWnd, IDC_WHEEL_Z3, edit);
	sprintf(edit, "%f", wheels[2].radius);
	SetDlgItemText(hWnd, IDC_WHEEL_R3, edit);
	sprintf(edit, "%f", wheels[2].theta_xz);
	SetDlgItemText(hWnd, IDC_WHEEL_THETA_XZ3, edit);
	sprintf(edit, "%f", wheels[2].theta_xy);
	SetDlgItemText(hWnd, IDC_WHEEL_THETA_XY3, edit);

	sprintf(edit, "%f", wheels[3].centre.x);
	SetDlgItemText(hWnd, IDC_WHEEL_X4, edit);
	sprintf(edit, "%f", wheels[3].centre.y);
	SetDlgItemText(hWnd, IDC_WHEEL_Y4, edit);
	sprintf(edit, "%f", wheels[3].centre.z);
	SetDlgItemText(hWnd, IDC_WHEEL_Z4, edit);
	sprintf(edit, "%f", wheels[3].radius);
	SetDlgItemText(hWnd, IDC_WHEEL_R4, edit);
	sprintf(edit, "%f", wheels[3].theta_xz);
	SetDlgItemText(hWnd, IDC_WHEEL_THETA_XZ4, edit);
	sprintf(edit, "%f", wheels[3].theta_xy);
	SetDlgItemText(hWnd, IDC_WHEEL_THETA_XY4, edit);

        break;

    case WM_COMMAND:
	switch (LOWORD(wParam))
	{
	case IDOK:
	    GetDlgItemText(hWnd, IDC_WHEEL_X1, edit, 128);
	    wheels[0].centre.x = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_Y1, edit, 128);
	    wheels[0].centre.y = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_Z1, edit, 128);
	    wheels[0].centre.z = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_R1, edit, 128);
	    wheels[0].radius = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_THETA_XZ1, edit, 128);
	    wheels[0].theta_xz = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_THETA_XY1, edit, 128);
	    wheels[0].theta_xy = atof(edit);

	    GetDlgItemText(hWnd, IDC_WHEEL_X2, edit, 128);
	    wheels[1].centre.x = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_Y2, edit, 128);
	    wheels[1].centre.y = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_Z2, edit, 128);
	    wheels[1].centre.z = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_R2, edit, 128);
	    wheels[1].radius = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_THETA_XZ2, edit, 128);
	    wheels[1].theta_xz = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_THETA_XY2, edit, 128);
	    wheels[1].theta_xy = atof(edit);

	    GetDlgItemText(hWnd, IDC_WHEEL_X3, edit, 128);
	    wheels[2].centre.x = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_Y3, edit, 128);
	    wheels[2].centre.y = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_Z3, edit, 128);
	    wheels[2].centre.z = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_R3, edit, 128);
	    wheels[2].radius = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_THETA_XZ3, edit, 128);
	    wheels[2].theta_xz = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_THETA_XY3, edit, 128);
	    wheels[2].theta_xy = atof(edit);

	    GetDlgItemText(hWnd, IDC_WHEEL_X4, edit, 128);
	    wheels[3].centre.x = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_Y4, edit, 128);
	    wheels[3].centre.y = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_Z4, edit, 128);
	    wheels[3].centre.z = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_R4, edit, 128);
	    wheels[3].radius = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_THETA_XZ4, edit, 128);
	    wheels[3].theta_xz = atof(edit);
	    GetDlgItemText(hWnd, IDC_WHEEL_THETA_XY4, edit, 128);
	    wheels[3].theta_xy = atof(edit);

            EndDialog(hWnd, 1);
	    return 1;

	case IDCANCEL:
	    EndDialog(hWnd, 0);
	    return 1;
	}

	break;
    }

    return 0;
}



/*
 * Draw view on dc.
 */
void
draw_stuff(HDC hdc, RECT rc)
{
    Point3D	*cs;
    double	y, z;
    int         i;
    int		r = GetDeviceCaps(hdc, LOGPIXELSX) / 30;

    if (view_plan)
    {
	if (results_view)
        {
            TextOut(hdc, r, r, echo1, strlen(echo1));
            TextOut(hdc, r, 7*r, echo2, strlen(echo2));
            TextOut(hdc, r, 13*r, echo3, strlen(echo3));

            for (i = 0; i < n_lines; i++)
	        draw_cp_curve(hdc, rc, FALSE, i);
            draw_cp_curve(hdc, rc, TRUE, zline);
        }

        if (show_unselected)
        {
	    if (list != NULL)
	    {
	        for (cs = list; cs != NULL; cs = cs->next_in_section)
	        {
		    draw_curve_zx(hdc, cs, rc, cs == currve && !printing, 1);
		    draw_curve_zx(hdc, cs, rc, cs == currve && !printing, -1);
	        }
	    }	    
        }
        else
        {
            if (currve != NULL)
            {
    		draw_curve_zx(hdc, currve, rc, !printing, 1);
		draw_curve_zx(hdc, currve, rc, !printing, -1);
            }
        }

	if (show_contours_zx)
	{
	    for (y = contour_start_zx; y <= contour_end_zx; y += contour_inc_zx)
	    {
		draw_contour_zx(hdc, rc, y, 1);
		draw_contour_zx(hdc, rc, y, -1);
	    }
	}

        draw_wheels_zx(hdc, rc);

	if (print_preview && !printing)
	{
	    draw_print_preview(hdc, rc, 1, 0);
	}
    }
    else if (view_elev)
    {
	if (results_view)
        {
            TextOut(hdc, r, r, echo1, strlen(echo1));
            TextOut(hdc, r, 7*r, echo2, strlen(echo2));
            TextOut(hdc, r, 13*r, echo3, strlen(echo3));

            for (i = 0; i < n_lines; i++)
	        draw_cp_curve(hdc, rc, FALSE, i);
            draw_cp_curve(hdc, rc, TRUE, zline);
        }

        if (show_unselected)
        {
	    if (list != NULL)
	    {
	        for (cs = list; cs != NULL; cs = cs->next_in_section)
		    draw_curve_zy(hdc, cs, rc, cs == currve);
	    }	    
        }
        else
        {
            if (currve != NULL)
		draw_curve_zy(hdc, currve, rc, !printing);
        }

	draw_wheels_zy(hdc, rc);

	if (print_preview && !printing)
	{
	    draw_print_preview(hdc, rc, 0, 1);
	}
    }
    else if (view_devel)
    {
        if (currve != NULL)
	    draw_devel(hdc, rc, currve);
    }
    else
    {
	if (show_unselected)
	{
	    if (list != NULL)
	    {
		for (cs = list; cs != NULL; cs = cs->next)
		{
		    draw_section(hdc, cs, rc, cs == current && !printing, 1);
		    draw_section(hdc, cs, rc, cs == current && !printing, -1);
		}
	    }	    
	}
	else
	{
	    if (current != NULL)
	    {
		draw_section(hdc, current, rc, !printing, 1);
		draw_section(hdc, current, rc, !printing, -1);
	    }
	}

	if (show_contours_xy)
	{
	    for (z = contour_start_xy; z <= contour_end_xy; z += contour_inc_xy)
	    {
		draw_contour_xy(hdc, rc, z, 1);
		draw_contour_xy(hdc, rc, z, -1);
	    }
	}

	draw_wheels_xy(hdc, rc);

	if (print_preview && !printing)
	{
	    draw_print_preview(hdc, rc, 0, 0);
	}
    }
}


/*
 * Prod the 3D window to get it to redraw (timers are turned off by
 * deactivation in the modified TK, to stop menus being repeatedly
 * redrawn, although I can't think why they should be. Must be a bug.)
 */
void
redraw_3D(void)
{
    if (auxGetHWND() != NULL)
        SendMessage(auxGetHWND(), WM_TIMER, 0, 0);
}


/*
 * Main window procedure.
 */
int WINAPI
lofty_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    FILE    *input;
    int	    first = 1;
    double  x, y, z, txy, tz;
    int	    u, v, w, h;
    Point3D *c;
    Point3D *cp = NULL;
    Point3D *cs = NULL;
    PAINTSTRUCT	ps;
    HDC	    hdc;
    RECT    rc;
    HMENU   hMenu, hMenuPopup;
    int	    cmd;
    POINT   pt;
    int     i, j, k, l, nw;
    char    ch;
    char    sect_title[128];
    char    junk[128];
    double  bx, by, scale;
    PRINTDLG	pd;
    int         elts, results;
    double      dynamic_press;
    int     elno;
    double  v0;
    int     view_section;
    int     show_contours;

    static OPENFILENAME	    ofn;
    static char		    file_title[64];
    static Point3D	    *drag = NULL;
    static int		    what = 0;
    static BOOL		    modified = FALSE;
    static char		    coords[32] = "\0";
    static char		    image[256];
    static char		    image_title[64];
    static double	    prev_x, prev_y, prev_z;
    static int		    modifier = 0;

    switch (msg)
    {
    case WM_CREATE:
	np = 0;
	filename[0] = '\0';
	list = NULL;
	view3D = FALSE;
	view_plan = FALSE;
	view_elev = FALSE;
	view_devel = FALSE;
        xmin = 0;
        xmax = default_xmax;
        ymin = default_ymin;
        ymax = default_ymax;
        zmin = 0;
        zmax = 0;
        memset(&ofn, 0, sizeof(OPENFILENAME));

	/*
	 * Get default printer printable size.
	 */
	memset(&pd, 0, sizeof(pd));
	pd.lStructSize = sizeof(pd);
	pd.hwndOwner   = hWnd;
	pd.hDevMode    = NULL;
	pd.hDevNames   = NULL;
	pd.Flags       = PD_RETURNDEFAULT | PD_RETURNIC; 
	pd.nCopies     = 1;
	pd.nFromPage   = 1; 
	pd.nToPage     = 0xFFFF;
	pd.nMinPage    = 1; 
	pd.nMaxPage    = 0xFFFF;

	if (PrintDlg(&pd)) 
	{
            double  x_size = GetDeviceCaps(pd.hDC, HORZRES);
            double  y_size = GetDeviceCaps(pd.hDC, VERTRES);
            double  x_ppm = 1000 * GetDeviceCaps(pd.hDC, LOGPIXELSX) / 25.4;
            double  y_ppm = 1000 * GetDeviceCaps(pd.hDC, LOGPIXELSY) / 25.4;

	    print_x_size = x_size / x_ppm;
	    print_y_size = y_size / y_ppm;
	    print_dm = pd.hDevMode;
	    print_dn = pd.hDevNames;
	    DeleteDC(pd.hDC);
	}

	return 0;

    case PROCESS_FILE:
    process_file:
        input = fopen(filename, "r");
	GetFileTitle(filename, file_title, sizeof(file_title));
	if (input != NULL)
	    goto opened_file;

	break;

    case WM_INITMENUPOPUP:
        elts = e != NULL ? MF_ENABLED : MF_GRAYED;
        results = (e != NULL && solved) ? MF_ENABLED : MF_GRAYED;
        view_section = !(view_plan || view_elev || view_devel);
        if (auxGetHWND() == NULL)
            view3D = FALSE;

        if (view_section)
            show_contours = show_contours_xy;
        else if (view_plan)
            show_contours = show_contours_zx;
        else
            show_contours = FALSE;

	switch (LOWORD(lParam))
	{
	case 0:
	    EnableMenuItem((HMENU)wParam, IDC_REVERT, modified ? MF_ENABLED : MF_GRAYED);
	    CheckMenuItem((HMENU)wParam, IDC_PRINT_PREVIEW, print_preview ? MF_CHECKED: MF_UNCHECKED);
	    break;

	case 1:
	    EnableMenuItem((HMENU)wParam, IDC_3DVIEW, np > 0 || nels > 0 ? MF_ENABLED : MF_GRAYED);
	    EnableMenuItem((HMENU)wParam, IDC_LOCK_BACKGROUND, view_section && current != NULL && current->background != NULL ? MF_ENABLED : MF_GRAYED);
	    EnableMenuItem((HMENU)wParam, IDC_BACKGROUND_IMAGE, view_section && current != NULL ? MF_ENABLED : MF_GRAYED);
	    EnableMenuItem((HMENU)wParam, IDC_SHOW_BACKGROUND, view_section && current != NULL ? MF_ENABLED : MF_GRAYED);
	    EnableMenuItem((HMENU)wParam, IDC_SHOW_CONTOURS, view_section || view_plan ? MF_ENABLED : MF_GRAYED);

            CheckMenuItem((HMENU)wParam, IDC_VIEW_SECTION, view_section ? MF_CHECKED: MF_UNCHECKED);
            CheckMenuItem((HMENU)wParam, IDC_VIEW_PLAN, view_plan ? MF_CHECKED: MF_UNCHECKED);
            CheckMenuItem((HMENU)wParam, IDC_VIEW_ELEV, view_elev ? MF_CHECKED: MF_UNCHECKED);
            CheckMenuItem((HMENU)wParam, IDC_VIEW_DEVEL, view_devel ? MF_CHECKED: MF_UNCHECKED);
	    CheckMenuItem((HMENU)wParam, IDC_3DVIEW, view3D ? MF_CHECKED : MF_UNCHECKED);
	    CheckMenuItem((HMENU)wParam, IDC_3D_RESULTS, results_view ? MF_CHECKED : MF_UNCHECKED);
	    CheckMenuItem((HMENU)wParam, IDC_SHOW_UNSEL, show_unselected ? MF_CHECKED : MF_UNCHECKED);
	    CheckMenuItem((HMENU)wParam, IDC_SHOW_BACKGROUND, show_background ? MF_CHECKED : MF_UNCHECKED);
	    CheckMenuItem((HMENU)wParam, IDC_SHOW_CONTOURS, show_contours ? MF_CHECKED : MF_UNCHECKED);
	    CheckMenuItem((HMENU)wParam, IDC_LOCK_BACKGROUND, lock_bgnd ? MF_CHECKED : MF_UNCHECKED);
	    break;
	    
	case 2:
	    EnableMenuItem((HMENU)wParam, IDC_SOLVE, elts);
	    EnableMenuItem((HMENU)wParam, IDC_SOLVE_AGAIN, elts);
	    EnableMenuItem((HMENU)wParam, IDC_ELEMENTS, elts);
	    EnableMenuItem((HMENU)wParam, IDC_NORMALS, elts);
	    EnableMenuItem((HMENU)wParam, IDC_ZLINES, elts);
	    EnableMenuItem((HMENU)wParam, IDC_NEXT_ZLINE, elts);
	    EnableMenuItem((HMENU)wParam, IDC_PREV_ZLINE, elts);
	    EnableMenuItem((HMENU)wParam, IDC_VEL_MAG, results);
	    EnableMenuItem((HMENU)wParam, IDC_VEL_VEC, results);
	    EnableMenuItem((HMENU)wParam, IDC_VEL_VEC_ONE_SIDE, results);
	    EnableMenuItem((HMENU)wParam, IDC_VEL_VEC_ONE_ZLINE, results);
	    EnableMenuItem((HMENU)wParam, IDC_VEL_NORM, results);
	    EnableMenuItem((HMENU)wParam, IDC_PRESSURE, results);
	    EnableMenuItem((HMENU)wParam, IDC_TURB_SEP_ZONES, results);

	    CheckMenuItem((HMENU)wParam, IDC_ELEMENTS, show_elements ? MF_CHECKED : MF_UNCHECKED);
	    CheckMenuItem((HMENU)wParam, IDC_NORMALS, show_normals ? MF_CHECKED : MF_UNCHECKED);
	    CheckMenuItem((HMENU)wParam, IDC_VEL_MAG, show_velocity_mag ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem((HMENU)wParam, IDC_VEL_VEC, 
                (show_velocity_vec && show_what == IDC_VEL_VEC) ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem((HMENU)wParam, IDC_VEL_VEC_ONE_SIDE, 
                (show_velocity_vec && show_what == IDC_VEL_VEC_ONE_SIDE) ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem((HMENU)wParam, IDC_VEL_VEC_ONE_ZLINE, 
                (show_velocity_vec && show_what == IDC_VEL_VEC_ONE_ZLINE) ? MF_CHECKED : MF_UNCHECKED);
	    CheckMenuItem((HMENU)wParam, IDC_VEL_NORM, show_velocity_norm ? MF_CHECKED : MF_UNCHECKED);
	    CheckMenuItem((HMENU)wParam, IDC_PRESSURE, show_pressures ? MF_CHECKED : MF_UNCHECKED);
	    CheckMenuItem((HMENU)wParam, IDC_ZLINES, show_zlines ? MF_CHECKED : MF_UNCHECKED);
	    CheckMenuItem((HMENU)wParam, IDC_ZLINES, show_zones ? MF_CHECKED : MF_UNCHECKED);
	    break;
	}

	break;

    case WM_LBUTTONDOWN:
	/*
	 * Clicking on a displayed section (or curve) selects it.
	 */
	GetClientRect(hWnd, &rc);
	modifier = wParam;
        if (view_plan)
        {
	    cs = hit_test_xz(LOWORD(lParam), HIWORD(lParam), rc, &drag, &what);
	    if (cs != NULL)
	    {
	        current = cs;
	        InvalidateRect(hWnd, NULL, TRUE);
	    }
	    if (drag != NULL)
	    {
		for 
		(
		    currve = list; 
		    currve != NULL && cs != drag; 
		    currve = currve->next_in_section, cs = cs->next_in_section
		)
		    ;
	        InvalidateRect(hWnd, NULL, TRUE);
	    }
	    SetCapture(hWnd);
	    translate_xz(LOWORD(lParam), HIWORD(lParam), rc, &prev_x, &prev_z);
        }
        else if (view_elev)
        {
	    cs = hit_test_yz(LOWORD(lParam), HIWORD(lParam), rc, &drag, &what);
	    if (cs != NULL)
	    {
	        current = cs;
	        InvalidateRect(hWnd, NULL, TRUE);
	    }
	    if (drag != NULL)
	    {
		for 
		(
		    currve = list; 
		    currve != NULL && cs != drag; 
		    currve = currve->next_in_section, cs = cs->next_in_section
		)
		    ;
	        InvalidateRect(hWnd, NULL, TRUE);
	    }
	    SetCapture(hWnd);
	    translate_yz(LOWORD(lParam), HIWORD(lParam), rc, &prev_y, &prev_z);
        }
        else if (view_devel)
	{
	    break;
	}
	else
        {
	    cs = hit_test_xy(LOWORD(lParam), HIWORD(lParam), rc, &drag, &what);
	    if (cs != NULL)
	    {
	        current = cs;
	        InvalidateRect(hWnd, NULL, TRUE);
	    }
	    SetCapture(hWnd);
	    translate_xy(LOWORD(lParam), HIWORD(lParam), rc, &prev_x, &prev_y);
        }

	break;

    case WM_MOUSEMOVE:
	GetClientRect(hWnd, &rc);
	if (view_plan)
	{
	    translate_xz(LOWORD(lParam), HIWORD(lParam), rc, &x, &z);

	    if (drag != NULL)
	    {
		modified = TRUE;
		switch (what)
		{
		case -1:	    /* parent point - drag all if SHIFT */
		    if (modifier & MK_SHIFT)
		    {
			for (cs = current; cs != NULL; cs = cs->next_in_section)
			    cs->z += z - prev_z;
			prev_z = z;
		    }
		    else
		    {
			drag->z = z;
		    }
		    break;
		case 1:		    /* ctrl[1] */
		    drag_cp_xz(drag, 1, 3, x, z);
		    break;
		case 3:		    /* ctrl[3] */
		    drag_cp_xz(drag, 3, 1, x, z);
		    break;
		}
		modified = TRUE;
		subdivide(hWnd, TRUE);
		InvalidateRect(hWnd, NULL, TRUE);
	    }
	}
	else if (view_elev)
	{
	    translate_yz(LOWORD(lParam), HIWORD(lParam), rc, &y, &z);

	    if (drag != NULL)
	    {
		modified = TRUE;
		switch (what)
		{
		case -1:	    /* parent point - drag all if SHIFT */
		    if (modifier & MK_SHIFT)
		    {
			for (cs = current; cs != NULL; cs = cs->next_in_section)
			    cs->z += z - prev_z;
			prev_z = z;
		    }
		    else
		    {
			drag->z = z;
		    }
		    break;
		case 1:		    /* ctrl[1] */
		    drag_cp_yz(drag, 1, 3, y, z);
		    break;
		case 3:		    /* ctrl[3] */
		    drag_cp_yz(drag, 3, 1, y, z);
		    break;
		}
		modified = TRUE;
		subdivide(hWnd, TRUE);
		InvalidateRect(hWnd, NULL, TRUE);
	    }
	}
	else if (view_devel)
	{
	    break;
	}
	else    /* section view */
	{
	    translate_xy(LOWORD(lParam), HIWORD(lParam), rc, &x, &y);
	    if (current != NULL)
		sprintf(coords, "(%f, %f, %f)", x, y, current->z);
	    else
		sprintf(coords, "(%f, %f)", x, y);

	    if (drag != NULL)
	    {
		modified = TRUE;
		switch (what)
		{
		case -1:	    /* parent point */
		    if (modifier & MK_SHIFT)
		    {
			/*
			 * Drag all (one side - the other mirrors) if SHIFT
			 */
			for (cs = current; cs != NULL; cs = cs->next_in_section)
			{
			    if (!cs->hard)
			    {
				if (!cs->midline)
				    cs->x += x - prev_x;
				cs->y += y - prev_y;
			    }
			}
			prev_x = x;
			prev_y = y;
		    }
		    else if (modifier & MK_CONTROL)
		    {
			/*
			 * Shrink and expand section about centre, if CTRL
			 * // TODO: Think about reducing tensions in user_moved points?
			 */
			double y0 = (ymin + ymax) / 2;
			double d0 = sqrt(prev_x*prev_x + (prev_y-y0)*(prev_y-y0));
			double d = sqrt(x*x + (y-y0)*(y-y0));

			for (cs = current; cs != NULL; cs = cs->next_in_section)
			{
			    if (!cs->hard)
			    {
				cs->x = cs->x * (d / d0);
				cs->y = (cs->y - y0) * (d / d0) + y0;
			    }
			}
			prev_x = x;
			prev_y = y;
		    }
		    else if (!drag->hard)
		    {
			if (!drag->midline)
			    drag->x = x;
			drag->y = y;
		    }
		    break;

		case 0:		    /* ctrl[0] */
		    drag_cp_xy(drag, 0, 2, x, y);
		    break;

		case 2:		    /* ctrl[2] */
		    drag_cp_xy(drag, 2, 0, x, y);
		    break;
		}
		modified = TRUE;
		subdivide(hWnd, TRUE);
		InvalidateRect(hWnd, NULL, TRUE);
	    }
	    else if (current != NULL && current->background != NULL && !lock_bgnd && GetCapture() != NULL)
	    {
		/*
		 * Drag background image, if there is one, and it is unlocked.
		 */
		current->bx += x - prev_x;
		current->by += y - prev_y;
		prev_x = x;
		prev_y = y;
		InvalidateRect(hWnd, NULL, TRUE);
	    }
	    else
	    {
		/*
		 * Any old mouse move - update the (x,y,z) display
		 */
		rc.left = 0;
		rc.top = 0;
		rc.right = 400;
		rc.bottom = 55;
		InvalidateRect(hWnd, &rc, TRUE);
	    }
	}
	
	break;

    case WM_LBUTTONUP:
	if (drag != NULL)
	{
	    drag = NULL;
	    modifier = 0;
	    ReleaseCapture();
	    subdivide(hWnd, FALSE);
	    InvalidateRect(hWnd, NULL, TRUE);
	}

	break;

    case WM_CONTEXTMENU:
	pt.x = LOWORD(lParam);
	pt.y = HIWORD(lParam);
	ScreenToClient(hWnd, &pt);
	GetClientRect(hWnd, &rc);
	
	if (view_plan)
	    cs = hit_test_xz(pt.x, pt.y, rc, &drag, &what);
	else if (view_elev)
	    cs = hit_test_yz(pt.x, pt.y, rc, &drag, &what);
	else if (view_devel)
	    break;
	else
	    cs = hit_test_xy(pt.x, pt.y, rc, &drag, &what);

	if (drag != NULL)
	{
            if (cs != current)
            {
                current = cs;
	        InvalidateRect(hWnd, NULL, TRUE);
            }

	    hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENU2));
	    switch (what)
	    {
	    case -1:
		hMenuPopup = GetSubMenu(hMenu, 0);
		CheckMenuItem(hMenuPopup, IDC_CUSP, drag->cusp ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(hMenuPopup, IDC_CUSP_CURVE, drag->cusp_curve ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(hMenuPopup, IDC_ENDPT, drag->endpt ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(hMenuPopup, IDC_HARD_PT, drag->hard ? MF_CHECKED : MF_UNCHECKED);
		break;
	    default:
		hMenuPopup = GetSubMenu(hMenu, 1);
		CheckMenuItem(hMenuPopup, IDC_USER_MOVED, drag->ctrl[what].user_moved ? MF_CHECKED : MF_UNCHECKED);
		break;
	    }

	    cmd = TrackPopupMenu
		(
		    hMenuPopup, 
		    TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, 
		    LOWORD(lParam), 
		    HIWORD(lParam), 
		    0, 
		    hWnd, 
		    NULL
		);
	    
	    DestroyMenu(hMenu);

	    switch (what)
	    {
	    case -1:
		switch (cmd)
		{
		case IDC_DELETE:
		    delete_point(drag, current);
		    modified = TRUE;
		    break;

		case IDC_DELETE_SECTION:
		    delete_section(current);
		    modified = TRUE;
		    break;

		case IDC_MOVE_POINT:
                    if
                    (
		        DialogBoxParam
			(
			    hInst, 
			    MAKEINTRESOURCE(POINT_DLG), 
			    hWnd, 
			    point_dialog, 
			    (LPARAM)drag
			)
                    )
                        modified = TRUE;

		    break;

		case IDC_MOVE_SECTION:
		    if
                    (
                        DialogBoxParam
			(
			    hInst, 
			    MAKEINTRESOURCE(SECTION_DLG), 
			    hWnd, 
			    section_dialog, 
			    (LPARAM)current
			)
                    )
                        modified = TRUE;

		    break;

		case IDC_ENDPT:
		    drag->endpt = !drag->endpt;
		    modified = TRUE;
		    break;

		case IDC_HARD_PT:
		    drag->hard = !drag->hard;
		    modified = TRUE;
		    break;

		case IDC_CUSP:
		    drag->cusp = !drag->cusp;
		    modified = TRUE;
		    break;

                case IDC_CUSP_CURVE:
		    drag->cusp_curve = !drag->cusp_curve;
		    modified = TRUE;
		    break;
		}
		break;

	    case 0:
	    case 2:
		if (cmd == IDC_USER_MOVED)
		{
		    drag->ctrl[what].user_moved = !drag->ctrl[what].user_moved;
		    if (!drag->cusp)
			drag->ctrl[2 - what].user_moved = !drag->ctrl[2 - what].user_moved;
		    modified = TRUE;
		}
		break;

	    case 1:
	    case 3:
		if (cmd == IDC_USER_MOVED)
		{
		    drag->ctrl[what].user_moved = !drag->ctrl[what].user_moved;
		    if (!drag->cusp_curve)
			drag->ctrl[4 - what].user_moved = !drag->ctrl[4 - what].user_moved;
		    modified = TRUE;
		}
		break;
	    }

	    drag = NULL;
	}
	else
	{
	    hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENU2));
	    hMenuPopup = GetSubMenu(hMenu, 2);
            EnableMenuItem(hMenuPopup, IDC_NEW, current != NULL ? MF_ENABLED : MF_GRAYED);
	    cmd = TrackPopupMenu
		(
		    hMenuPopup, 
		    TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, 
		    LOWORD(lParam), 
		    HIWORD(lParam), 
		    0, 
		    hWnd, 
		    NULL
		);
	    
	    DestroyMenu(hMenu);
	    translate_xy(pt.x, pt.y, rc, &x, &y);
	    switch (cmd)
	    {
	    case IDC_NEW:
	        insert_point(x, y, current);    /* Only if have a current point */
		modified = TRUE;
		break;

	    case IDC_NEW_SECTION:
		{
		    Point3D  dummy;

		    dummy.x = 0;
		    dummy.y = 0;
		    dummy.z = 0;
                    dummy.tension_xy = tension_xy;
                    dummy.tension_z = tension_z;
		    dummy.next_in_section = NULL;
		    dummy.sect_name = malloc(128);
		    dummy.sect_name[0] = '\0';
		    modified = DialogBoxParam
			(
			    hInst, 
			    MAKEINTRESOURCE(SECTION_DLG), 
			    hWnd, 
			    section_dialog, 
			    (LPARAM)&dummy
			);
		    if (modified)
		    {
			tension_xy = dummy.tension_xy;
			tension_z = dummy.tension_z;
			current = insert_section(x, y, dummy.z, dummy.sect_name);
		    }
		    free(dummy.sect_name);
		}
		break;
	    }
	}

	subdivide(hWnd, FALSE);
	InvalidateRect(hWnd, NULL, TRUE);
	break;

    case WM_COMMAND:
	switch (LOWORD(wParam))
	{
	case IDC_OPEN:
	    if (!check_before_closing(modified, hWnd))
		break;

	    /*
	     * Open an input file.
	     */
	    ofn.lStructSize = sizeof(OPENFILENAME);
	    ofn.hwndOwner = hWnd;
	    ofn.lpstrFilter = "Sections (*.lft)\0*.lft\0All Files (*.*)\0*.*\0";
	    ofn.lpstrTitle = "Open a Section File";
	    ofn.lpstrFile = filename;
	    ofn.nMaxFile = sizeof(filename);
	    ofn.lpstrFileTitle = file_title;
	    ofn.nMaxFileTitle = sizeof(file_title);
	    ofn.Flags = OFN_READONLY | OFN_FILEMUSTEXIST;
	    ofn.lpstrDefExt = "lft";

	    if (!GetOpenFileName(&ofn))
		break;

	    /* 
	     * Read the cross-sections in from the file.
	     * The Z direction is assumed to be the longitudinal, so the
	     * cross sections lie in the X-Y plane (all points in a given
	     * cross section have the same Z value)
	     * Link them "vertically" in cross sections.
	     */
	    input = fopen(filename, "r");

	opened_file:
	    fgets(title, 128, input);
	    title[strlen(title) - 1] = '\0';    /* blank out LF */
	    np = 0;
	    ns = 1;
	    nw = 0;

	    while (fscanf(input, "%c ", &ch) != EOF)
	    {
		switch (ch)
		{
		case 's':			/* section header */
		    fgets(sect_title, 128, input);
		    sect_title[strlen(sect_title) - 1] = '\0';    /* blank out LF */
		    j = -1;
		    break;

		case 'p':			/* section point */
		    fscanf
                    (
                        input, 
                        "%lf %lf %lf %d %d %d %d %lf %lf\n", 
                        &x, &y, &z, &u, &v, &w, &h, &txy, &tz
                    );

		    c = malloc(sizeof(Point3D));
		    c->x = x;
		    c->y = y;
		    c->z = z;
		    c->t = skin_thickness;
		    c->next_section_head = NULL;
		    c->next_in_section = NULL;
		    c->prev_in_section = NULL;
		    c->prev = c->next = NULL;
		    c->cusp = u;
                    c->cusp_curve = v;
		    c->endpt = w;
		    c->hard = h;
		    c->tension_xy = txy;
		    c->tension_z = tz;

		    c->midline = (x < 1.0e-5);
		    c->oncurve0.x = 0;
		    c->oncurve0.y = 0;
		    c->oncurve0.z = 0;
		    c->oncurve2.x = 0;
		    c->oncurve2.y = 0;
		    c->oncurve2.z = 0;
		    for (i = 0; i < 4; i++)
		    {
			c->ctrl[i].x = 0;
			c->ctrl[i].y = 0;
			c->ctrl[i].z = 0;
			c->ctrl[i].user_moved = 0;
		    }
		    c->background_file = NULL;
		    c->background = NULL;
		    c->hpal = NULL;

		    if (list == NULL)   /* initialise list head */
			list = c;

		    if (cs == NULL)	    /* and head of this cross section */
			cs = c;

		    if (j < 0)
		    {
			c->sect_name = malloc(128);
			strcpy(c->sect_name, sect_title);
			if (j == -2)
			{
			    c->background_file = malloc(64);
			    strcpy(c->background_file, image_title);
			    c->bx = bx;
			    c->by = by;
			    c->scale = scale;
			    
			    c->background = read_bmp(image_title);
			    c->hpal = bmp_palette(c->background);
			    
			    j = -1;
			}
		    }

		    if (cp != NULL)	    /* we have a current point... */
		    {
			if (j == -1)	    /* changed cross section, link back to cs */
			{
			    cs->next_section_head = c;
			    cs = c;
			    ns++;
			}
			else  /* same cross section */
			{
			    cp->next_in_section = c;
                            c->prev_in_section = cp;
			}
		    }

		    cp = c;
		    j = 0;
		    
		    break;
		case 't':		    /* thickness for last section point */
		    fscanf(input, "%lf\n", &c->t);
		    if (c->t < skin_thickness)
			c->t = skin_thickness;
		    break;

		case 'c':		    /* control point for last section point */
                    fscanf(input, "%lf %lf %lf %d\n", 
			    &c->ctrl[j].x, &c->ctrl[j].y, &c->ctrl[j].z, &c->ctrl[j].user_moved);
		    j++;
		    break;

		case 'b':                   /* background image this section */
		    if (j == -1)
		    {
			fscanf(input, "%lf %lf %lf %s\n", &bx, &by, &scale, image_title);
			j = -2;
		    }
		    break;
		case 'w':		    /* wheel */
		    fscanf(input, "%lf %lf %lf %lf %lf %lf\n",
			    &wheels[nw].centre.x, &wheels[nw].centre.y, &wheels[nw].centre.z,
			    &wheels[nw].radius, &wheels[nw].theta_xz, &wheels[nw].theta_xy);
		    nw++;
		    break;
		default:		    /* unsupported stuff just read the rest of the line */
		    fgets(junk, 128, input);
		    break;
		}
	    }

	    fclose(input);
	    modified = FALSE;
            nels = 0;
            solved = FALSE;
	    nnode = 0;

	    /*
	     * Generate 3D curves and elements.
	     */
	    results_view = FALSE;
	    current = list;
            currve = list;
	    subdivide(hWnd, FALSE);

	    /*
	     * Display everything.
	     */
	    SetWindowText(hWnd, title);
	    InvalidateRect(hWnd, NULL, TRUE);
	    break;

	case IDC_CLOSE:
	    check_before_closing(modified, hWnd);
	    break;

	case IDC_REVERT:
	    if (filename[0] != '\0')
	    {
		np = 0;
		list = NULL;
		InvalidateRect(hWnd, NULL, TRUE);
		goto process_file;
	    }
	    break;

	case IDC_SAVE_AS:
	save_as:
	    ofn.lStructSize = sizeof(OPENFILENAME);
	    ofn.hwndOwner = hWnd;
	    ofn.lpstrFilter = "Sections (*.lft)\0*.lft\0All Files (*.*)\0*.*\0";
	    ofn.lpstrTitle = "Save a Section File";
	    ofn.lpstrFile = filename;
	    ofn.nMaxFile = sizeof(filename);
	    ofn.lpstrFileTitle = file_title;
	    ofn.nMaxFileTitle = sizeof(file_title);
	    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
	    ofn.lpstrDefExt = "lft";

	    if (!GetSaveFileName(&ofn))
		break;

	    /* fallthrough */
	case IDC_SAVE:
	    if (filename[0] == '\0')
		goto save_as;

	    save_file();
	    modified = FALSE;
	    
	    break;

        case IDC_PREFERENCES:
	    if 
	    (
		DialogBox
		(
		    hInst, 
		    MAKEINTRESOURCE(PREFERENCES_DLG), 
		    hWnd, 
		    pref_dialog
		)
	    )
	    {
		modified = TRUE;
		InvalidateRect(hWnd, NULL, TRUE);
	    }

            break;

        case IDC_WHEELS:
	    if 
	    (
		DialogBox
		(
		    hInst, 
		    MAKEINTRESOURCE(WHEEL_DLG), 
		    hWnd, 
		    wheel_dialog
		)
	    )
	    {
		modified = TRUE;
		InvalidateRect(hWnd, NULL, TRUE);
	    }

            break;

        case IDC_HELP_ABOUT:
	    DialogBox
	    (
		hInst, 
		MAKEINTRESOURCE(ABOUT_DLG), 
		hWnd, 
		about_dialog
	    );
            break;

	case IDC_BACKGROUND_IMAGE:
	    ofn.lStructSize = sizeof(OPENFILENAME);
	    ofn.hwndOwner = hWnd;
	    ofn.lpstrFilter = "Images (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0";
	    ofn.lpstrTitle = "Open a Background Image File";
	    ofn.lpstrFile = image;
	    ofn.nMaxFile = sizeof(image);
	    ofn.lpstrFileTitle = image_title;
	    ofn.nMaxFileTitle = sizeof(image_title);
	    ofn.Flags = OFN_READONLY | OFN_FILEMUSTEXIST;
	    ofn.lpstrDefExt = "bmp";

	    if (!GetOpenFileName(&ofn))
		break;

	    modified = TRUE;
	    if (current->background_file == NULL)
		current->background_file = malloc(64);
	    strcpy(current->background_file, image_title);

	    current->background = read_bmp(image);
	    current->hpal = bmp_palette(current->background);
	    current->bx = 0;
	    current->by = ymax;
	    current->scale = background_image_scale;
	    if (current->background != NULL)
		InvalidateRect(hWnd, NULL, TRUE);

	    break;

	case IDC_LOCK_BACKGROUND:
	    lock_bgnd = !lock_bgnd;
	    break;

        case IDC_SHOW_UNSEL:
            show_unselected = !show_unselected;
            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case IDC_SHOW_BACKGROUND:
            show_background = !show_background;
            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case IDC_SHOW_CONTOURS:
            view_section = !(view_plan || view_elev);
            if (view_section)
                show_contours_xy = !show_contours_xy;
            else if (view_plan)
                show_contours_zx = !show_contours_zx;

            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case IDC_NEXT:
	    if (view_plan || view_elev || view_devel)
	    {
		if (currve != NULL && currve->next_in_section != NULL)
		{
		    currve = currve->next_in_section;
		    InvalidateRect(hWnd, NULL, TRUE);
		}
	    }
	    else
	    {
		if (current != NULL && current->next != NULL)
		{
	            current = current->next;
		    InvalidateRect(hWnd, NULL, TRUE);
		}
	    }
	    break;

	case IDC_PREVIOUS:
	    if (view_plan || view_elev || view_devel)
	    {
		if (currve != NULL && currve->prev_in_section != NULL)
		{
		    currve = currve->prev_in_section;
		    InvalidateRect(hWnd, NULL, TRUE);
		}
	    }
	    else
	    {
		if (current != NULL && current->prev != NULL)
		{
		    current = current->prev;
		    InvalidateRect(hWnd, NULL, TRUE);
		}
	    }
	    break;

	case IDC_PRINT_PREVIEW:
	    print_preview = !print_preview;
	    InvalidateRect(hWnd, NULL, TRUE);
	    break;

        case IDC_VIEW_SECTION:
            view_plan = FALSE;
            view_elev = FALSE;
	    view_devel = FALSE;
	    InvalidateRect(hWnd, NULL, TRUE);
            break;

        case IDC_VIEW_PLAN:
            view_plan = TRUE;
            view_elev = FALSE;
	    view_devel = FALSE;
	    InvalidateRect(hWnd, NULL, TRUE);
            break;

        case IDC_VIEW_ELEV:
            view_plan = FALSE;
            view_elev = TRUE;
	    view_devel = FALSE;
	    InvalidateRect(hWnd, NULL, TRUE);
            break;

        case IDC_VIEW_DEVEL:
            view_plan = FALSE;
            view_elev = FALSE;
	    view_devel = TRUE;
	    InvalidateRect(hWnd, NULL, TRUE);
            break;

        case IDC_3D_RESULTS:
	    results_view = !results_view;
	    InvalidateRect(hWnd, NULL, TRUE);
            redraw_3D();
	    break;

        case IDC_ZLINES:
	    show_zlines = !show_zlines;
            redraw_3D();
	    break;

        case IDC_NEXT_ZLINE:
	    if (zline < n_lines - 1)
		zline++;
	    InvalidateRect(hWnd, NULL, TRUE);
            redraw_3D();
	    break;

        case IDC_PREV_ZLINE:
	    if (zline > 0)
		zline--;
	    InvalidateRect(hWnd, NULL, TRUE);
            redraw_3D();
	    break;

	case IDC_3DVIEW:
	    if (view3D)
	    {
		ShowWindow(auxGetHWND(), SW_HIDE);
		view3D = FALSE;
		break;
	    }

	    /*
	     * Display the 3D visualisation.
	     */
	    view3D = TRUE;
	    if (auxGetHWND() != NULL)
	    {
		ShowWindow(auxGetHWND(), SW_SHOW);
	    }
	    else
	    {
                wWidth = wHeight = GetSystemMetrics(SM_CYFULLSCREEN);
		auxInitPosition(0, 0, wWidth, wHeight);
		auxInitDisplayMode(AUX_DEPTH16 | AUX_RGB | AUX_DOUBLE);
		auxInitWindow(file_title);
                SetClassLong(auxGetHWND(), GCL_HICON, 
                            (long)LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1)));
		Init();        

		auxExposeFunc((AUXEXPOSEPROC)Reshape);
		auxReshapeFunc((AUXRESHAPEPROC)Reshape);
    
		auxKeyFunc( AUX_z, Key_z ); // zero viewing distance
		auxKeyFunc( AUX_r, Key_r ); // results view
		auxKeyFunc( AUX_e, Key_e ); // elements
		auxKeyFunc( AUX_n, Key_n ); // normals
		auxKeyFunc( AUX_m, Key_m ); // vel. magnitudes
		auxKeyFunc( AUX_v, Key_v ); // vel. vectors
		auxKeyFunc( AUX_p, Key_p ); // pressures
		auxKeyFunc( AUX_l, Key_l ); // zlines
		auxKeyFunc( AUX_t, Key_t ); // zones of turb/separation
		auxKeyFunc( AUX_UP, Key_up );	    // up zline
		auxKeyFunc( AUX_DOWN, Key_down );   // down zline

		auxMouseFunc( AUX_LEFTBUTTON, AUX_MOUSEDOWN, trackball_MouseDown );
		auxMouseFunc( AUX_LEFTBUTTON, AUX_MOUSEUP, trackball_MouseUp );
		auxMouseFunc( AUX_RIGHTBUTTON, AUX_MOUSEDOWN, size_down );
		auxMouseFunc( AUX_RIGHTBUTTON, AUX_MOUSEUP, size_up );
    
		trackball_Init( wWidth, wHeight );
	    }

	    auxIdleFunc(Draw);
	    auxMainLoop(Draw);

	    break;

        case IDC_SOLVE:
	    hWndSolve = CreateDialog
			(
			    hInst, 
			    MAKEINTRESOURCE(SOLVE_DLG), 
			    hWnd, 
			    solve_dialog
			);

	    ShowWindow(hWndSolve, SW_SHOW);
	    break;

	case IDC_SOLVE_AGAIN:
	    /*
	     * If we need, regenerate the influence coefficients.
	     */
	    if (!solved)
            {
                subdivide(hWnd, TRUE);
		populate();
            }

            /*
             * Solve the normal equations for the source density distribution.
             */
            solve();

            /* 
             * Calculate the final velocity field and pressures.
             */
            vel_min = 999999;
            vel_max = -999999;
            norm_min = 999999;
            norm_max = -999999;
            press_min = 999999;
            press_max = -999999;
            v0 = dist(vinf);
            for (i = 0; i < nels; i++)
            {
                e[i].vel = vinf;

                for (j = 0; j < nels; j++)
                {
                    /* 
                     * Velocity vectors.
                     */
                    e[i].vel.u += e[i].infl[j].V.u * e[j].sigma;
                    e[i].vel.v += e[i].infl[j].V.v * e[j].sigma;
                    e[i].vel.w += e[i].infl[j].V.w * e[j].sigma;

                }

                /* 
                 * Velocity magnitudes.
                 */
                e[i].vel_mag = dist(e[i].vel);

                /* 
                 * Velocity normals (these are an error check - they should be 0)
                 */
                e[i].vel_norm =
                    e[i].vel.u * e[i].xf.m31
                    +
                    e[i].vel.v * e[i].xf.m32
                    +
                    e[i].vel.w * e[i].xf.m33;

                /*
                 * Pressure coefficients. 
                 */
                e[i].press = 1 - ((e[i].vel_mag * e[i].vel_mag) / (v0 * v0));
		e[i].separated = FALSE;
		e[i].turb = FALSE;

                /*
                 * Accumulate scales and limits for the quantities that
                 * can be plotted.
                 */
                vel_min = min(vel_min, e[i].vel_mag);
                vel_max = max(vel_max, e[i].vel_mag);
                norm_min = min(norm_min, e[i].vel_norm);
                norm_max = max(norm_max, e[i].vel_norm);
                press_min = min(press_min, e[i].press);
                press_max = max(press_max, e[i].press);
            }

	    if (use_turb)
	    {
		/*
		 * Compute turbulent transition. This is based on the critical Reynolds' number calculated
		 * along the path length along the Z-line. (yes, I know the velocities don't always follow
		 * the Z-lines exactly, but what the hey). 
		 */
		double critl = viscosity * reynolds_turb / (v0 * density);
		double total_length;
		int	last_sep;

		for (l = 0; l < n_lines; l++)
		{
		    /*
		     * Compute path length from tail, then reverse the path lengths 
		     * so zero is at the nose. Set the turb flag if this length exceeds
		     * the critical length. Only one side so we can see the pressures
                     * on the other side in the 3D-view.
		     */
		    k = line_start[l];
		    e[k].path_length = 0;
		    while (e[k].skip_to_next != 0)
		    {
			j = k + e[k].skip_to_next;
			e[j].path_length = e[k].path_length + e[k].dim;   // careful - dim must be in Z!
			k = j;
		    }

		    total_length = e[j].path_length;
		    k = line_start[l];
		    e[k].turb = total_length - e[k].path_length > critl;
		    while (e[k].skip_to_next != 0)
		    {
			j = k + e[k].skip_to_next;
			e[j].turb = total_length - e[j].path_length > critl;
			k = j;
		    }
		}

		/*
		 * Compute separation zones. These are based on the slope of Cp vs. Z, along the Z-lines. 
		 * The slope is tested against one of two thresholds:
		 * - zero/small, if flow is laminar (the turbulent boundary-layer transition has not happened)
		 * - some positive threshold, if turbulent.
		 * In the separation zone the pressure is reduced to Pmin / 2.
                 * Set both sides this time.
		 *
		 * NOTE: the Z-lines start at the tail and move toward the nose.
		 */
		for (l = 0; l < n_lines; l++)
		{
                    int     prevk;

		    prevk = k = line_start[l];
		    last_sep = -1;
		    while (e[k].skip_to_next != 0)
		    {
			j = k + e[k].skip_to_next;

			if 
			(
			    (e[k].press - e[j].press) / (e[j].o.z - e[k].o.z) 
			    > 
			    (e[k].turb ? press_slope_turb : press_slope_lam) / (zmax - zmin)
			)
			{
			    e[k].separated = TRUE;
			    e[k].press = press_min / 2;
			    e[k+nels/2].separated = TRUE;	/* the other side too */
			    e[k+nels/2].press = press_min / 2;

                            if (e[k].separated && e[prevk].turb)
			        last_sep = j;
			}
                        prevk = k;
			k = j;
		    }

		    /*
		     * Don't allow reattachment if already turbulent. 
		     */
		    if (last_sep != -1)
		    {
			k = line_start[l];
			while (e[k].skip_to_next != 0)
			{
			    j = k + e[k].skip_to_next;

			    if (k < last_sep) 
			    {
				e[k].separated = TRUE;
				e[k].press = press_min / 2;
				e[k+nels/2].separated = TRUE;	/* the other side too */
				e[k+nels/2].press = press_min / 2;
			    }
			    k = j;
			}
		    }
		}
	    }

            /*
             * Calculate the forces acting - drag (later - centre of pressure, moments)
             *
             * Total force acting is sum of pressure * area, in the direction of vinf,
             * and sum of components of skin drag, in the direction of vinf.
             */
            dynamic_press = 0.5 * density * v0 * v0;
            total_force.u = 0;
            total_force.v = 0;
            total_force.w = 0;
	    surface_area = 0;
	    total_mass = 0;
            for (i = 0; i < nels; i++)
            {
                double     re, cf, skinf;

                if (!e[i].body)
                    continue;

                /*
                 * Pressure-induced forces
                 */
                total_force.u += e[i].press * e[i].area * dynamic_press * e[i].xf.m31;
                total_force.v += e[i].press * e[i].area * dynamic_press * e[i].xf.m32;
                total_force.w += e[i].press * e[i].area * dynamic_press * e[i].xf.m33;

                if (use_skin && !e[i].separated)
                {
                    /*
                     * Skin-friction forces based on von Karman approximation to Cf.
                     * (see http://adg.stanford.edu/aa241/drag/skinfriction.html)
                     * Reynolds no. is based on total length.
                     * Wetted area doesn't include separated zones.
                     */
                    re = e[i].vel_mag * (zmax - zmin) * density / viscosity;
                    cf = 0.455 / pow(log10(re), 2.58);
                    skinf = 0.5 * e[i].area * cf * density * SQ(e[i].vel_mag);

                    total_force.u -= skinf * e[i].vel.u / e[i].vel_mag;
                    total_force.v -= skinf * e[i].vel.v / e[i].vel_mag;
                    total_force.w -= skinf * e[i].vel.w / e[i].vel_mag;
                }

                /*
                 * While here, sum up the total surface area and the mass.
		 * ( /1000 because the thickneses are in mm)
                 */
		surface_area += e[i].area;
		skin_mass = e[i].area * skin_thickness * bulk_density / 1000;
		infill_mass = 
		    e[i].area
		    * 
		    (
			(node[e[i].n[0]].t + node[e[i].n[1]].t + node[e[i].n[2]].t + node[e[i].n[3]].t) / 4
			-
			skin_thickness * 2
		    )
		    *
		    (bulk_density * infill_percentage / 100)
		    /
		    1000;
		if (infill_mass < 0)
		    infill_mass = 0;

		total_mass += skin_mass + infill_mass;
            }

	    force = -dot(total_force, vinf) / v0;
	    cd = 2 * force / (frontal_area * density * v0 * v0);
	    power = force * v0;
            sprintf(echo1, "%s (%s)", title, filename);
            sprintf
            (
                echo2, 
                "V0 (%.1lf, %.1lf, %.1lf) m/s Density=%.3lf Visc=%lf", 
                vinf.u, vinf.v, vinf.w,
                density,
                viscosity
            );
	    sprintf
	    (
		echo3, 
		"Surf=%.2lfm2 Front=%.2lfm2 Mass=%.2lfkg Re=%.0lf Cd=%.2lf Drag=%.0lfN Power=%.0lfW", 
		surface_area, 
		frontal_area, 
		total_mass,
                v0 * (zmax - zmin) * density / viscosity,
		cd,
		force,
		power
	    );

            solved = TRUE;
	    results_view = TRUE;
	    if (!view3D)
		SendMessage(hWnd, WM_COMMAND, IDC_3DVIEW, 0);
            else
                redraw_3D();

            break;
        
        case IDC_ELEMENTS:
	    show_elements = !show_elements;
            redraw_3D();
	    break;

        case IDC_NORMALS:
	    show_normals = !show_normals;
            redraw_3D();
	    break;

        case IDC_VEL_MAG:
	    show_velocity_mag = TRUE;
            show_what = LOWORD(wParam);
            show_velocity_vec = FALSE;
            show_velocity_norm = FALSE;
            show_pressures = FALSE;
            redraw_3D();
	    break;

        case IDC_VEL_VEC:
        case IDC_VEL_VEC_ONE_SIDE:
        case IDC_VEL_VEC_ONE_ZLINE:
	    show_velocity_vec = TRUE;
            show_what = LOWORD(wParam);
            show_velocity_mag = FALSE;
            show_velocity_norm = FALSE;
            show_pressures = FALSE;
            redraw_3D();
	    break;

        case IDC_VEL_NORM:
	    show_velocity_norm = TRUE;
            show_what = LOWORD(wParam);
            show_velocity_mag = FALSE;
            show_velocity_vec = FALSE;
            show_pressures = FALSE;
            redraw_3D();
	    break;

        case IDC_PRESSURE:
	    show_pressures = TRUE;
            show_what = LOWORD(wParam);
            show_velocity_mag = FALSE;
            show_velocity_vec = FALSE;
            show_velocity_norm = FALSE;
            redraw_3D();
	    break;

	case IDC_TURB_SEP_ZONES:
	    show_zones = !show_zones;
            redraw_3D();
	    break;

	case IDC_PAGE_SETUP:
	    memset(&pd, 0, sizeof(pd));
	    pd.lStructSize = sizeof(pd);
	    pd.hwndOwner   = hWnd;
	    pd.hDevMode    = print_dm;
	    pd.hDevNames   = print_dn;
	    pd.Flags       = PD_PRINTSETUP | PD_RETURNIC; 
	    pd.nCopies     = 1;
	    pd.nFromPage   = 1; 
	    pd.nToPage     = 0xFFFF;
	    pd.nMinPage    = 1; 
	    pd.nMaxPage    = 0xFFFF;

	    if (PrintDlg(&pd)) 
	    {
		double  x_size = GetDeviceCaps(pd.hDC, HORZRES);
		double  y_size = GetDeviceCaps(pd.hDC, VERTRES);
		double  x_ppm = 1000 * GetDeviceCaps(pd.hDC, LOGPIXELSX) / 25.4;
		double  y_ppm = 1000 * GetDeviceCaps(pd.hDC, LOGPIXELSY) / 25.4;

		print_x_size = x_size / x_ppm;
		print_y_size = y_size / y_ppm;
		print_dm = pd.hDevMode;
		print_dn = pd.hDevNames;
		DeleteDC(pd.hDC);
	    }
	
	    break;	
	
	case IDC_PRINT:
	    memset(&pd, 0, sizeof(pd));
	    pd.lStructSize = sizeof(pd);
	    pd.hwndOwner   = hWnd;
	    pd.hDevMode    = print_dm;
	    pd.hDevNames   = print_dn;
	    pd.Flags       = PD_USEDEVMODECOPIESANDCOLLATE | PD_RETURNDC; 
	    pd.nCopies     = 1;
	    pd.nFromPage   = 1; 
	    pd.nToPage     = 0xFFFF;
	    pd.nMinPage    = 1; 
	    pd.nMaxPage    = 0xFFFF;

	    if (PrintDlg(&pd)) 
	    {
		DOCINFO	di;
		int	nError;
                int     x_size, y_size, xc, yc;
                double  x_ppm, y_ppm;
                int     page_step_x, page_step_y;
                int     overlap_x, overlap_y;
                int     n_pages_x, n_pages_y;
                int     x_page, y_page, n_page;
                char    page_num[160], coord[16];
		int	single_page;

		print_dm = pd.hDevMode;
		print_dn = pd.hDevNames;

		memset(&di, 0, sizeof(DOCINFO));
		di.cbSize = sizeof(DOCINFO); 
		di.lpszDocName = "Lofty"; 
		di.lpszOutput = (LPTSTR) NULL; 
		di.lpszDatatype = (LPTSTR) NULL; 
		di.fwType = 0; 
 		nError = StartDoc(pd.hDC, &di); 
		if (nError == SP_ERROR) 
		    break; 
 
		marg = 0;
                printing = TRUE;

                rc.left = 0;
		rc.top = 0;
                x_size = GetDeviceCaps(pd.hDC, HORZRES);
                y_size = GetDeviceCaps(pd.hDC, VERTRES);
                x_ppm = 1000 * GetDeviceCaps(pd.hDC, LOGPIXELSX) / 25.4;
                y_ppm = 1000 * GetDeviceCaps(pd.hDC, LOGPIXELSY) / 25.4;

                /*
                 * Large pixel rect for whole drawing.
                 */
		single_page = 
		    fit_to_page 
		    || 
		    (results_view && (view_plan || view_elev));
                if (single_page)
		{
		    rc.right = x_size;
		    rc.bottom = y_size;
                    n_pages_x = 1;
                    n_pages_y = 1;
		}
		else
		{
		    if (view_plan)
		    {
			rc.right = (int)(((zmax - zmin) / print_scale) * x_ppm);
			rc.bottom = (int)(((2 * xmax) / print_scale) * y_ppm);
		    }
		    else if (view_elev || view_devel)
		    {
			rc.right = (int)(((zmax - zmin) / print_scale) * x_ppm);
			rc.bottom = (int)(((ymax - ymin) / print_scale) * y_ppm);
		    }
		    else    /* section view */
		    {
			rc.right = (int)(((2 * xmax) / print_scale) * x_ppm);
			rc.bottom = (int)(((ymax - ymin) / print_scale) * y_ppm);
		    }

                    /*
                     * Overlap the pages so they contain at least one grid line
                     * in common. Make sure the page step is an exact multiple of
                     * the grid cell.
                     */
                    if (grid_step != 0)
                    {
                        overlap_x = (int)(grid_step * x_ppm / print_scale);
                        overlap_y = (int)(grid_step * y_ppm / print_scale);
                    }
                    else
                    {
                        overlap_x = (int)(0.01 * x_ppm / print_scale);
                        overlap_y = (int)(0.01 * y_ppm / print_scale);
                    }

                    page_step_x = (int)(floor((x_size - overlap_x) / overlap_x) * overlap_x);
                    page_step_y = (int)(floor((y_size - overlap_y) / overlap_y) * overlap_y);
                    n_pages_x = (rc.right - overlap_x + page_step_x - 1) / page_step_x;
                    n_pages_y = (rc.bottom - overlap_y + page_step_y - 1) / page_step_y;
                }

		/*
		 * Page loops.
		 */
                for (n_page = 1, x_page = 0; x_page < n_pages_x; x_page++)
                {
                    for (y_page = 0; y_page < n_pages_y; y_page++, n_page++)
                    {
                        if (n_page >= pd.nFromPage && n_page <= pd.nToPage)
                        {
                            nError = StartPage(pd.hDC); 
		            if (nError <= 0) 
		                break; 
 
			    if (!single_page && grid_step != 0)
                            {
				int	thick = max(2, GetDeviceCaps(pd.hDC, LOGPIXELSX) / 160);
				HPEN	thick_pen = CreatePen(PS_SOLID, thick, RGB(0, 0, 0));
				HPEN	black_pen = GetStockObject(BLACK_PEN);
				HPEN	old_pen = SelectObject(pd.hDC, black_pen);

                                /*
                                 * Draw the grid. Thick lines every 100mm. Different loop bounds for
				 * different views, hence the spaghetti.
                                 */
				if (view_plan)
				{
				    for (x = 0; x < xmax; x += grid_step)
				    {
					yc = rc.top + (int)((x + xmax) * x_ppm / print_scale);
					if ((x * 10) - floor(x * 10 + 0.0001) < 0.0001)
					{
					    sprintf(coord, "%.0f", x * 1000);
					    TextOut(pd.hDC, rc.left, yc, coord, strlen(coord));
					    SelectObject(pd.hDC, thick_pen);
					}
					else
					{
					    SelectObject(pd.hDC, black_pen);
					}
					MoveToEx(pd.hDC, rc.left, yc, NULL);
					LineTo(pd.hDC, rc.right, yc);

					yc = rc.top + (int)((-x + xmax) * x_ppm / print_scale);
					if ((-x * 10) - floor(-x * 10 + 0.0001) < 0.0001)
					{
					    sprintf(coord, "%.0f", -x * 1000);
					    TextOut(pd.hDC, rc.left, yc, coord, strlen(coord));
					}
					MoveToEx(pd.hDC, rc.left, yc, NULL);
					LineTo(pd.hDC, rc.right, yc);
				    }

				    for (z = floor(zmin / grid_step) * grid_step; z < zmax; z += grid_step)
				    {
					xc = rc.left + (int)((z - zmin) * x_ppm / print_scale);
					if ((z * 10) - floor(z * 10 + 0.0001) < 0.0001)
					{
					    sprintf(coord, "%.0f", z * 1000);
					    TextOut(pd.hDC, xc, rc.top, coord, strlen(coord));
					    SelectObject(pd.hDC, thick_pen);
					}
					else
					{
					    SelectObject(pd.hDC, black_pen);
					}
					MoveToEx(pd.hDC, xc, rc.top, NULL);
					LineTo(pd.hDC, xc, rc.bottom);
				    }
				}
				else if (view_elev || view_devel)
				{
				    for (y = floor(ymin / grid_step) * grid_step; y < ymax; y += grid_step)
				    {
					yc = rc.bottom - (int)((y - ymin) * y_ppm / print_scale);
					if ((y * 10) - floor(y * 10 + 0.0001) < 0.0001)
					{
					    sprintf(coord, "%.0f", y * 1000);
					    TextOut(pd.hDC, rc.left, yc, coord, strlen(coord));
					    SelectObject(pd.hDC, thick_pen);
					}
					else
					{
					    SelectObject(pd.hDC, black_pen);
					}
					MoveToEx(pd.hDC, rc.left, yc, NULL);
					LineTo(pd.hDC, rc.right, yc);
				    }

				    for (z = floor(zmin / grid_step) * grid_step; z < zmax; z += grid_step)
				    {
					xc = rc.left + (int)((z - zmin) * x_ppm / print_scale);
					if ((z * 10) - floor(z * 10 + 0.0001) < 0.0001)
					{
					    sprintf(coord, "%.0f", z * 1000);
					    TextOut(pd.hDC, xc, rc.top, coord, strlen(coord));
					    SelectObject(pd.hDC, thick_pen);
					}
					else
					{
					    SelectObject(pd.hDC, black_pen);
					}
					MoveToEx(pd.hDC, xc, rc.top, NULL);
					LineTo(pd.hDC, xc, rc.bottom);
				    }
				}
				else	/* section view */
				{
				    for (y = floor(ymin / grid_step) * grid_step; y < ymax; y += grid_step)
				    {
					yc = rc.bottom - (int)((y - ymin) * y_ppm / print_scale);
					if ((y * 10) - floor(y * 10 + 0.0001) < 0.0001)
					{
					    sprintf(coord, "%.0f", y * 1000);
					    TextOut(pd.hDC, rc.left, yc, coord, strlen(coord));
					    SelectObject(pd.hDC, thick_pen);
					}
					else
					{
					    SelectObject(pd.hDC, black_pen);
					}
					MoveToEx(pd.hDC, rc.left, yc, NULL);
					LineTo(pd.hDC, rc.right, yc);
				    }

				    for (x = 0; x < xmax; x += grid_step)
				    {
					xc = rc.left + (int)((x + xmax) * x_ppm / print_scale);
					if ((x * 10) - floor(x * 10 + 0.0001) < 0.0001)
					{
					    sprintf(coord, "%.0f", x * 1000);
					    TextOut(pd.hDC, xc, rc.top, coord, strlen(coord));
					    SelectObject(pd.hDC, thick_pen);
					}
					else
					{
					    SelectObject(pd.hDC, black_pen);
					}
					MoveToEx(pd.hDC, xc, rc.top, NULL);
					LineTo(pd.hDC, xc, rc.bottom);

					xc = rc.left + (int)((-x + xmax) * x_ppm / print_scale);
					if ((-x * 10) - floor(-x * 10 + 0.0001) < 0.0001)
					{
					    sprintf(coord, "%.0f", -x * 1000);
					    TextOut(pd.hDC, xc, rc.top, coord, strlen(coord));
					}
					MoveToEx(pd.hDC, xc, rc.top, NULL);
					LineTo(pd.hDC, xc, rc.bottom);
				    }
				}

				SelectObject(pd.hDC, old_pen);
				DeleteObject(thick_pen);
                            }

                            sprintf(page_num, "%d  %s", n_page, title);
                            TextOut(pd.hDC, 0, 0, page_num, strlen(page_num));

			    draw_stuff(pd.hDC, rc);

		            EndPage(pd.hDC); 
                        }

                        rc.top -= page_step_y;
                        rc.bottom -= page_step_y;
                    }

                    rc.top += n_pages_y * page_step_y;
                    rc.bottom += n_pages_y * page_step_y;
                    rc.left -= page_step_x;
                    rc.right -= page_step_x;
                }

                EndDoc(pd.hDC); 
		DeleteDC(pd.hDC);
                marg = 30;
                printing = FALSE;
	    }
	    break;

        case IDC_PRINT3D:
	    memset(&pd, 0, sizeof(pd));
	    pd.lStructSize = sizeof(pd);
	    pd.hwndOwner   = hWnd;
	    pd.hDevMode    = NULL;
	    pd.hDevNames   = NULL;
	    pd.Flags       = PD_USEDEVMODECOPIESANDCOLLATE | PD_RETURNDC; 
	    pd.nCopies     = 1;
	    pd.nFromPage   = 1; 
	    pd.nToPage     = 0xFFFF;
	    pd.nMinPage    = 1; 
	    pd.nMaxPage    = 0xFFFF;

	    if (PrintDlg(&pd)) 
	    {
		DOCINFO	di;
		int	nError;
                HGLRC   rc;
                int     w;

                memset(&di, 0, sizeof(DOCINFO));
		di.cbSize = sizeof(DOCINFO); 
		di.lpszDocName = "Lofty"; 
		di.lpszOutput = (LPTSTR) NULL; 
		di.lpszDatatype = (LPTSTR) NULL; 
		di.fwType = 0; 
 		nError = StartDoc(pd.hDC, &di); 
		if (nError == SP_ERROR) 
		    break; 
                nError = StartPage(pd.hDC); 
		if (nError <= 0) 
		    break; 
                
	        if (view3D)
	        {
		    auxCloseWindow();
		    view3D = FALSE;
	        }
                printing_3d = TRUE;         // Stop SwapBuffers in Draw()
                rc = wglCreateContext(pd.hDC);
                wglMakeCurrent(pd.hDC, rc);

                Init();
                w = GetDeviceCaps(pd.hDC, HORZRES);
                Reshape(w, w);
                Draw();

                wglMakeCurrent(NULL, NULL);
                wglDeleteContext(rc);
                printing_3d = FALSE;

                EndPage(pd.hDC); 
                EndDoc(pd.hDC); 
		DeleteDC(pd.hDC);
                GlobalFree(pd.hDevMode);
                GlobalFree(pd.hDevNames);
	    }
            break;

	case IDC_EXIT:
	    /*
	     * Exiting.
	     */
	    if (!check_before_closing(modified, hWnd))
		break;
	    goto get_out;
	}
	break;

    case WM_WINDOWPOSCHANGED:
	InvalidateRect(hWnd, NULL, TRUE);
	break;

    case WM_ERASEBKGND:
	if (view_plan || view_elev || view_devel || current == NULL || !show_background || current->background == NULL)
	    return DefWindowProc(hWnd, msg, wParam, lParam);

	hdc = (HDC)wParam;
	GetClientRect(hWnd, &rc);
	draw_bmp_and_white(hdc, rc, current->background, current->hpal, current->bx, current->by, current->scale);
	break;

    case WM_PAINT:
	hdc = BeginPaint(hWnd, &ps);
	GetClientRect(hWnd, &rc);

	if (view_plan)
	{
	    TextOut(hdc, 3, 3, "Plan", 4);
	}
	else if (view_elev)
	{
	    TextOut(hdc, 3, 3, "Elevation", 9);
	}
	else if (view_devel)
	{
	    TextOut(hdc, 3, 3, "Development", 11);
	}
	else
	{
	    draw_axes(hdc, rc);
	    TextOut(hdc, 3, 3, coords, strlen(coords));
	    if (current != NULL)
		TextOut(hdc, 3, 20, current->sect_name, strlen(current->sect_name));
	}

	draw_stuff(hdc, rc);
	EndPaint(hWnd, &ps);

	break;

    case WM_DESTROY:
	check_before_closing(modified, hWnd);
    get_out:
	auxCloseWindow();
	auxQuit();
	PostQuitMessage(0);
	break;

    default:
	return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    return 1;
}  
    


int WINAPI 
WinMain
(
    HINSTANCE hInstance,	// handle to current instance
    HINSTANCE hPrevInstance,	// handle to previous instance
    LPSTR lpCmdLine,	// pointer to command line
    int nCmdShow 	// show state of window
)
{
    MSG		msg; 
    WNDCLASS	wc; 
    HACCEL	hAccel;
    
    /* Register the window class for the main window. */ 
 
    if (!hPrevInstance) 
    { 
        wc.style = 0; 
        wc.lpfnWndProc = (WNDPROC)lofty_WndProc; 
        wc.cbClsExtra = 0; 
        wc.cbWndExtra = 0; 
        wc.hInstance = hInstance; 
        wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)); 
        wc.hCursor = LoadCursor((HINSTANCE) NULL, 
            IDC_ARROW); 
        wc.hbrBackground = GetStockObject(WHITE_BRUSH); 
        wc.lpszMenuName =  MAKEINTRESOURCE(IDR_MENU1); 
        wc.lpszClassName = "lofty_WndClass"; 
 
        if (!RegisterClass(&wc)) 
            return FALSE; 
    } 
 
    hInst = hInstance;  /* save instance handle */ 
 
    /* Create the main window. */ 
 
    hWndMain = CreateWindow("lofty_WndClass", "Lofty", 
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 
        CW_USEDEFAULT, CW_USEDEFAULT, (HWND) NULL, 
        (HMENU) NULL, hInst, (LPVOID) NULL); 
 
    /* 
     * If the main window cannot be created, terminate 
     * the application. 
     */ 
 
    if (!hWndMain) 
        return FALSE; 
 
    /* Show the window and paint its contents. */ 
 
    ShowWindow(hWndMain, nCmdShow); 
    UpdateWindow(hWndMain); 

    if (lpCmdLine != NULL && lpCmdLine[0] != '\0')
    {
	strcpy(filename, lpCmdLine);
	PostMessage(hWndMain, PROCESS_FILE, 0, 0);
    }

    hAccel = LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_ACCELERATOR1));

    /* Start the message loop. */ 
 
    while (GetMessage(&msg, (HWND) NULL, 0, 0)) 
    {
	if (!IsWindow(hWndSolve) || !IsDialogMessage(hWndSolve, &msg)) 
	{
            /*
             * Send accelerator keys straight to the 3D window, if it is in front.
             */
	    if (msg.hwnd == auxGetHWND() || !TranslateAccelerator(msg.hwnd, hAccel, &msg))
	    {
		TranslateMessage(&msg); 
		DispatchMessage(&msg); 
	    }
	}
    } 
 
    /* Return the exit code to Windows. */ 
 
    return msg.wParam; 
}
