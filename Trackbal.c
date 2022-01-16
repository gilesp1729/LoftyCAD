/*
 * Trackball code:
 *
 * Implementation of a virtual trackball.
 * Implemented by Gavin Bell, lots of ideas from Thant Tessman and
 *   the August '88 issue of Siggraph's "Computer Graphics," pp. 121-129.
 *
 * Vector manip code:
 *
 * Original code from:
 * David M. Ciemiewicz, Mark Grossman, Henry Moreton, and Paul Haeberli
 *
 * Much mucking with by:
 * Gavin Bell
 *
 * Shell hacking courtesy of:
 * Reptilian Inhaleware
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include <math.h>
#include <gl\gl.h>
#include <gl\glu.h>
#include "glaux\glaux.h"
#include "trackbal.h"
#include "LoftyCAD.h"

/* 
 * globals
 */
static GLenum (*MouseDownFunc)(int, int, GLenum) = NULL;
static GLenum (*MouseUpFunc)(int, int, GLenum)   = NULL;
static HWND ghwnd;
GLint giWidth, giHeight;
LONG    glMouseDownX, glMouseDownY;
BOOL    gbLeftMouse = FALSE;
BOOL    gbSpinning = FALSE;
float   curquat[4], lastquat[4];

// Slerp parameters.
float   start_quat[4], desired_quat[4];
BOOL    slerping = FALSE;
float   slerp_step = 0.1f;
float   slerp_t;

/*
 * This size should really be based on the distance from the center of
 * rotation to the point on the object underneath the mouse.  That
 * point would then track the mouse as closely as possible.  This is a
 * simple example, though, so that is left as an Exercise for the
 * Programmer.
 */
#define TRACKBALLSIZE  (0.8f)

/*
 * Local function prototypes (not defined in trackball.h)
 */
static float tb_project_to_sphere(float, float, float);
static void normalize_quat(float [4]);
void qcopy(const float *q1, float *q2);
BOOL slerp(float start[4], float desired[4], float result[4], float t);

void 
trackball_Init( GLint width, GLint height )
{
    ghwnd = auxGetHWND();
    giWidth = width;
    giHeight = height;

    trackball_calc_quat( curquat, 0.0f, 0.0f, 0.0f, 0.0f );
}

// Set a desired rotation, and start slerping round to it.
void
trackball_InitQuat(float quat[4])
{
    qcopy(curquat, start_quat);
    qcopy(quat, desired_quat);
    slerp_t = 0;
    slerping = TRUE;
}

void
trackball_Resize( GLint width, GLint height )
{
    giWidth = width;
    giHeight = height;
}

void CALLBACK
trackball_MouseDown( AUX_EVENTREC *event) 
{
    SetCapture(ghwnd);
    glMouseDownX = event->data[AUX_MOUSEX];
    glMouseDownY = event->data[AUX_MOUSEY];
    gbLeftMouse = TRUE;
}


void CALLBACK
trackball_MouseUp( AUX_EVENTREC *event ) 
{
    ReleaseCapture();
    gbLeftMouse = FALSE;
}

BOOL
trackball_IsOrbiting(void)
{
    return gbLeftMouse;
}


/* these 4 not used yet */
void
trackball_MouseDownEvent( int mouseX, int mouseY, GLenum button )
{
}

void
trackball_MouseUpEvent( int mouseX, int mouseY, GLenum button )
{
}

void 
trackball_MouseDownFunc(GLenum (*Func)(int, int, GLenum))
{
    MouseDownFunc = Func;
}

void 
trackball_MouseUpFunc(GLenum (*Func)(int, int, GLenum))
{
    MouseUpFunc = Func;
}

void
trackball_CalcRotMatrix( GLfloat matRot[4][4] )
{
    POINT pt;

    if (slerping)
    {
        gbSpinning = FALSE;
        slerp_t += slerp_step;
        slerping = slerp(start_quat, desired_quat, curquat, slerp_t);
    }
    else if (gbLeftMouse)
    {
        auxGetMouseLoc(&pt.x, &pt.y);
#ifdef DEBUG_TRACKBALL_SPIN
        {
            char buf[64];
            sprintf_s(buf, 64, "Left mouse down %d %d\r\n", pt.x, pt.y);
            Log(buf);
        }
#endif

        // If mouse has moved since button was pressed, change quaternion.

            if (pt.x != glMouseDownX || pt.y != glMouseDownY)
            {
#if 1
    /* negate all params for proper operation with glTranslate(-z)
     */
                trackball_calc_quat(lastquat,
                          -(2.0f * ( giWidth - glMouseDownX ) / giWidth - 1.0f),
                          -(2.0f * glMouseDownY / giHeight - 1.0f),
                          -(2.0f * ( giWidth - pt.x ) / giWidth - 1.0f),
                          -(2.0f * pt.y / giHeight - 1.0f)
                         );
#else
// now out-of-date
                trackball_calc_quat(lastquat,
                          2.0f * ( Width - glMouseDownX ) / Width - 1.0f,
                          2.0f * glMouseDownY / Height - 1.0f,
                          2.0f * ( Width - pt.x ) / Width - 1.0f,
                          2.0f * pt.y / Height - 1.0f );
#endif

                gbSpinning = TRUE;
            }
            else
            {
                gbSpinning = FALSE;
            }

            glMouseDownX = pt.x;
            glMouseDownY = pt.y;
    }

    if (gbSpinning)
        trackball_add_quats(lastquat, curquat, curquat);

    trackball_build_rotmatrix(matRot, curquat);
}

void
trackball_stop_spin()
{
    gbSpinning = FALSE;
}

void
vzero(float *v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
}

void
vset(float *v, float x, float y, float z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

void
vsub(const float *src1, const float *src2, float *dst)
{
    dst[0] = src1[0] - src2[0];
    dst[1] = src1[1] - src2[1];
    dst[2] = src1[2] - src2[2];
}

void
vcopy(const float *v1, float *v2)
{
    register int i;
    for (i = 0 ; i < 3 ; i++)
        v2[i] = v1[i];
}

void
vcross(const float *v1, const float *v2, float *cross)
{
    float temp[3];

    temp[0] = (v1[1] * v2[2]) - (v1[2] * v2[1]);
    temp[1] = (v1[2] * v2[0]) - (v1[0] * v2[2]);
    temp[2] = (v1[0] * v2[1]) - (v1[1] * v2[0]);
    vcopy(temp, cross);
}

float
vlength(const float *v)
{
    return (float) sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

void
vscale(float *v, float div)
{
    v[0] *= div;
    v[1] *= div;
    v[2] *= div;
}

void
vnormal(float *v)
{
    vscale(v,1.0f/vlength(v));
}

float
vdot(const float *v1, const float *v2)
{
    return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

void
vadd(const float *src1, const float *src2, float *dst)
{
    dst[0] = src1[0] + src2[0];
    dst[1] = src1[1] + src2[1];
    dst[2] = src1[2] + src2[2];
}

void
qcopy(const float *q1, float *q2)
{
    register int i;
    for (i = 0; i < 4; i++)
        q2[i] = q1[i];
}

float
qdot(const float *q1, const float *q2)
{
    return q1[0] * q2[0] + q1[1] * q2[1] + q1[2] * q2[2] + q1[3] * q2[3];
}

/*
 * Ok, simulate a track-ball.  Project the points onto the virtual
 * trackball, then figure out the axis of rotation, which is the cross
 * product of P1 P2 and O P1 (O is the center of the ball, 0,0,0)
 * Note:  This is a deformed trackball-- is a trackball in the center,
 * but is deformed into a hyperbolic sheet of rotation away from the
 * center.  This particular function was chosen after trying out
 * several variations.
 * 
 * It is assumed that the arguments to this routine are in the range
 * (-1.0 ... 1.0)
 */
void
trackball_calc_quat(float q[4], float p1x, float p1y, float p2x, float p2y)
{
    float a[3]; /* Axis of rotation */
    float phi;  /* how much to rotate about axis */
    float p1[3], p2[3], d[3];
    float t;

    if (p1x == p2x && p1y == p2y) {
	/* Zero rotation */
        vzero(q); 
	q[3] = 1.0f; 
        return;
    }

    /*
     * First, figure out z-coordinates for projection of P1 and P2 to
     * deformed sphere
     */
    vset(p1,p1x,p1y,tb_project_to_sphere(TRACKBALLSIZE,p1x,p1y));
    vset(p2,p2x,p2y,tb_project_to_sphere(TRACKBALLSIZE,p2x,p2y));

    /*
     *  Now, we want the cross product of P1 and P2
     */
    vcross(p2,p1,a);

    /*
     *  Figure out how much to rotate around that axis.
     */
    vsub(p1,p2,d);
    t = vlength(d) / (2.0f*TRACKBALLSIZE);

    /*
     * Avoid problems with out-of-control values...
     */
    if (t > 1.0f) t = 1.0f;
    if (t < -1.0f) t = -1.0f;
    phi = 2.0f * (float) asin(t);

    trackball_axis_to_quat(a,phi,q);
}

/*
 *  Given an axis and angle, compute quaternion.
 */
void
trackball_axis_to_quat(float a[3], float phi, float q[4])
{
    vnormal(a);
    vcopy(a,q);
    vscale(q,(float) sin(phi/2.0f));
    q[3] = (float) cos(phi/2.0f);
}

/*
 * Project an x,y pair onto a sphere of radius r OR a hyperbolic sheet
 * if we are away from the center of the sphere.
 */
static float
tb_project_to_sphere(float r, float x, float y)
{
    float d, t, z;

    d = (float) sqrt(x*x + y*y);
    if (d < r * 0.70710678118654752440f) {    /* Inside sphere */
	z = (float) sqrt(r*r - d*d);
    } else {           /* On hyperbola */
        t = r / 1.41421356237309504880f;
        z = t*t / d;
    }
    return z;
}

/*
 * Given two rotations, e1 and e2, expressed as quaternion rotations,
 * figure out the equivalent single rotation and stuff it into dest.
 * 
 * This routine also normalizes the result every RENORMCOUNT times it is
 * called, to keep error from creeping in.
 *
 * NOTE: This routine is written so that q1 or q2 may be the same
 * as dest (or each other).
 */

#define RENORMCOUNT 97

void
trackball_add_quats(float q1[4], float q2[4], float dest[4])
{
    static int count=0;
    float t1[4], t2[4], t3[4];
    float tf[4];

    vcopy(q1,t1); 
    vscale(t1,q2[3]);

    vcopy(q2,t2); 
    vscale(t2,q1[3]);

    vcross(q2,q1,t3);
    vadd(t1,t2,tf);
    vadd(t3,tf,tf);
    tf[3] = q1[3] * q2[3] - vdot(q1,q2);

    dest[0] = tf[0];
    dest[1] = tf[1];
    dest[2] = tf[2];
    dest[3] = tf[3];

    if (++count > RENORMCOUNT) {
        count = 0;
        normalize_quat(dest);
    }
}

// Slerp from start_quat to desired_quat in steps of t in [0,1]. Return FALSE whan done.
BOOL
slerp(float start[4], float desired[4], float result[4], float t)
{
    double theta, theta_0, s0, s1;
    double dot = qdot(start, desired);

    if (dot < 0.0f)                 // swap to go the short way round
    {
        start[0] = -start[0];
        start[1] = -start[1];
        start[2] = -start[2];
        start[3] = -start[3];
        dot = -dot;
    }

    if (dot > 0.99 || t > 0.95)
    {
        qcopy(desired, result);    // we're finished
        return FALSE;
    }

    theta_0 = acos(dot);    // theta_0 = angle between input vectors
    theta = theta_0 * t;    // theta = angle between v0 and result

    s0 = cos(theta) - dot * sin(theta) / sin(theta_0);  // == sin(theta_0 - theta) / sin(theta_0)
    s1 = sin(theta) / sin(theta_0);

    result[0] = (float)(s0 * start[0] + s1 * desired[0]);
    result[1] = (float)(s0 * start[1] + s1 * desired[1]);
    result[2] = (float)(s0 * start[2] + s1 * desired[2]);
    result[3] = (float)(s0 * start[3] + s1 * desired[3]);

    return TRUE;
}

#if 0
Quaternion slerp(Quaternion v0, Quaternion v1, double t) {
    // Only unit quaternions are valid rotations.
    // Normalize to avoid undefined behavior.
    v0.normalize();
    v1.normalize();

    // Compute the cosine of the angle between the two vectors.
    double dot = dot_product(v0, v1);

    // If the dot product is negative, the quaternions
    // have opposite handed-ness and slerp won't take
    // the shorter path. Fix by reversing one quaternion.
    if (dot < 0.0f) {
        v1 = -v1;
        dot = -dot;
    }

    const double DOT_THRESHOLD = 0.9995;
    if (dot > DOT_THRESHOLD) {
        // If the inputs are too close for comfort, linearly interpolate
        // and normalize the result.

        Quaternion result = v0 + t*(v1 � v0);
        result.normalize();
        return result;
    }

    Clamp(dot, -1, 1);           // Robustness: Stay within domain of acos()
    double theta_0 = acos(dot);  // theta_0 = angle between input vectors
    double theta = theta_0*t;    // theta = angle between v0 and result

    double s0 = cos(theta) - dot * sin(theta) / sin(theta_0);  // == sin(theta_0 - theta) / sin(theta_0)
    double s1 = sin(theta) / sin(theta_0);

    return (s0 * v0) + (s1 * v1);
}
#endif


/*
 * Quaternions always obey:  a^2 + b^2 + c^2 + d^2 = 1.0
 * If they don't add up to 1.0, dividing by their magnitued will
 * renormalize them.
 *
 * Note: See the following for more information on quaternions:
 * 
 * - Shoemake, K., Animating rotation with quaternion curves, Computer
 *   Graphics 19, No 3 (Proc. SIGGRAPH'85), 245-254, 1985.
 * - Pletinckx, D., Quaternion calculus as a basic tool in computer
 *   graphics, The Visual Computer 5, 2-13, 1989.
 */
static void
normalize_quat(float q[4])
{
    int i;
    float mag;

    mag = (q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    for (i = 0; i < 4; i++) q[i] /= mag;
}

/*
 * Build a rotation matrix, given a quaternion rotation.
 *
 */
void
trackball_build_rotmatrix(float m[4][4], float q[4])
{
    m[0][0] = 1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2]);
    m[0][1] = 2.0f * (q[0] * q[1] - q[2] * q[3]);
    m[0][2] = 2.0f * (q[2] * q[0] + q[1] * q[3]);
    m[0][3] = 0.0f;

    m[1][0] = 2.0f * (q[0] * q[1] + q[2] * q[3]);
    m[1][1]= 1.0f - 2.0f * (q[2] * q[2] + q[0] * q[0]);
    m[1][2] = 2.0f * (q[1] * q[2] - q[0] * q[3]);
    m[1][3] = 0.0f;

    m[2][0] = 2.0f * (q[2] * q[0] - q[1] * q[3]);
    m[2][1] = 2.0f * (q[1] * q[2] + q[0] * q[3]);
    m[2][2] = 1.0f - 2.0f * (q[1] * q[1] + q[0] * q[0]);
    m[2][3] = 0.0f;

    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}
