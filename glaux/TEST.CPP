#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <gl/glaux.h>
#include <conio.h>
#include <stdio.h>

int ClientWidth;
int ClientHeight;
bool fSolid = false;
float fromX = 3;
float fromY = 3;
float fromZ = 3;

void DoObjects( void )
{
   // SPHERE
   glColor3f(1, 0, 0);
   glPushMatrix();
   glScalef( 1, 1, 1 );
   glTranslatef( .5, 0, 0 );
   glRotatef( -90, 1, 0, 0 );
//   glRotatef(  0, 0, 1, 0 );
//   glRotatef(  0, 0, 0, 1 );
   if( !fSolid )
      {
      auxWireSphere( .50 );
      }
   else
      {
      auxSolidSphere( .50 );
      }
   glPopMatrix();

   // CONE
   glColor3f(1, 0, 1);
   glPushMatrix();
   glScalef( 1, 1, 1 );
   glTranslatef( -.5, -.5, 0 );
   glRotatef( -90, 1, 0, 0 );
//   glRotatef(  0, 0, 1, 0 );
//   glRotatef(  0, 0, 0, 1 );
   if( !fSolid )
      {
      auxWireCone( .5, 1.0 );
//      auxWireBox( .5, .5, .5 );
      }
   else
      {
      auxSolidCone( .5, 1.0 );
//      auxSolidBox( .5, .5, .5 );
      }
   glPopMatrix();

   // TORUS
   glColor3f(1, 1, 0);
   glPushMatrix();
   glScalef( 1, 1, 1 );
   glTranslatef( 0, 0, -1.0 );
//   glRotatef( -90, 1, 0, 0 );
//   glRotatef(  0, 0, 1, 0 );
//   glRotatef(  0, 0, 0, 1 );
   if( !fSolid )
      {
      auxWireTorus( .15, .5);
      }
   else
      {
      auxSolidTorus( .15, .5);
      }
   glPopMatrix();

   // CYLINDER
   glColor3f(0, 1, 1);
   glPushMatrix();
   glScalef( 1, 1, 1 );
   glTranslatef( -1, -.5, -1.0 );
//   glRotatef( -90, 1, 0, 0 );
//   glRotatef(  0, 0, 1, 0 );
//   glRotatef(  0, 0, 0, 1 );
   if( !fSolid )
      {
      auxWireCylinder(.3, 1);
      }
   else
      {
      auxSolidCylinder(.3, 1);
      }
   glPopMatrix();

   // CUBE
   glColor3f(0, 1, 1);
   glPushMatrix();
   glScalef( 1, 1, 1 );
   glTranslatef( 1, 0, -1.0 );
//   glRotatef( -90, 1, 0, 0 );
//   glRotatef(  0, 0, 1, 0 );
//   glRotatef(  0, 0, 0, 1 );
   if( !fSolid )
      {
      auxWireCube( .5 );
      }
   else
      {
      auxSolidCube( .5 );
      }
   glPopMatrix();
}

void DoView( void )
{
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

      {
      GLdouble aspect = ClientWidth;
      aspect /= ClientHeight;

//        glFrustum(-FProjectionWidth/2, FProjectionWidth/2, -FProjectionHeight/2, FProjectionHeight/2, FProjectionNear, FProjectionFar);
      gluPerspective( 35.0,
                      aspect,
		                .0001,
		                1000);
      }

   gluLookAt(  fromX, fromY,  fromZ,
               0, 0,  0,
               0, 1,  0 );
   glMatrixMode(GL_MODELVIEW);

   glLoadIdentity();

   glEnable(GL_DEPTH_TEST);
   glEnable(GL_CULL_FACE);
}

void DoLights( void )
{
   glEnable(GL_LIGHTING);

   glLightModeli( GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE );
   glEnable( GL_COLOR_MATERIAL );
   glColorMaterial(1, GL_DIFFUSE );
   glColorMaterial(1, GL_AMBIENT );
   glColorMaterial(0, GL_EMISSION );
   glColorMaterial(0, GL_SPECULAR );
   {
   GLfloat pos[4];
   pos[0] = 0;
   pos[1] = 0;
   pos[2] = 50;
   pos[3] = 1;
   glLightfv(GL_LIGHT0, GL_POSITION, pos);
   }
   {
   GLfloat diffuse[] = { 1.0,  1.0,  1.0,  1.0 };
   glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse);
   }
   glEnable( GL_LIGHT0 );

}

void CALLBACK MainProc( void )
{
   DoView();
   DoLights();
   DoObjects();

   glFlush();

   auxSwapBuffers();
}

void CALLBACK ReShape( GLsizei w, GLsizei h) 
{ 
   glViewport( 0, 0, w, h );
   ClientWidth = w;
   ClientHeight = h;
}

void UpdateInfo( void )
{
   clrscr();
   gotoxy( 1,1 );
   cprintf( "fromX: %f\r\n", fromX );
   cprintf( "fromY: %f\r\n", fromY );
   cprintf( "fromZ: %f\r\n", fromZ );
}

#ifdef EXGLAUX
void CALLBACK KeyProc( int key, int status )
{
   switch( key )
      {
      case 27:    auxQuit(); break;

      case 37:    fromX -= .2; MainProc( ); break;
      case 38:    fromZ -= .2; MainProc( ); break;
      case 39:    fromX += .2; MainProc( ); break;
      case 40:    fromZ += .2; MainProc( ); break;

      case 's':   fSolid = !fSolid; MainProc( ); break;

      default:    cprintf( "Unknown Key\r\n" ); break;
      }

   UpdateInfo( ); 
   cprintf( "Key: %d\r\n", key );
   cprintf( "Status: %d\r\n", status );
}

void CALLBACK MouseFunc( int X, int Y, int Button, int State, int Type )
{
   UpdateInfo( ); 
   cprintf("XY: %d %d\r\n", X, Y );
   switch( Button )
      {
      case AUX_LEFTBUTTON:    cprintf("Button: Left\r\n" );    break;
      case AUX_MIDDLEBUTTON:  cprintf("Button: Middle\r\n" );  break;
      case AUX_RIGHTBUTTON:   cprintf("Button: Right\r\n" );   break;
      default:                cprintf("Button: None\r\n" );    break;
      }

   cprintf("State: %d\r\n", State );
   switch( State )
      {
      case AUX_SHIFT:            cprintf("State: Shift\r\n" );          break;
      case AUX_CONTROL:          cprintf("State: Control\r\n" );        break;
      case AUX_SHIFT|AUX_CONTROL:cprintf("State: Shift&Control\r\n" );  break;
      default:                   cprintf("State: None\r\n" );           break;
      }
   switch ( Type )
      {
      case eMouseDown:           cprintf("Type: MouseDown\r\n" );       break;
      case eMouseUp:             cprintf("Type: MouseUp\r\n" );         break;
      case eMouseMove:           cprintf("Type: MouseMove\r\n" );       break;
      }
}
#else
void CALLBACK EscapeKeyProc( void ) { auxQuit(); }
void CALLBACK SKeyProc( void ) { fSolid = !fSolid; MainProc( ); }
void CALLBACK LeftKeyProc( void ) { fromX -= .2; MainProc( ); UpdateInfo( ); }
void CALLBACK RightKeyProc( void ) { fromX += .2; MainProc( ); UpdateInfo( ); }
void CALLBACK UpKeyProc( void ) { fromZ -= .2; MainProc( ); UpdateInfo( ); }
void CALLBACK DownKeyProc( void ) { fromZ += .2; MainProc( ); UpdateInfo( ); }
#endif

void main( void )
{
   auxInitPosition(10, 10, 310, 310);
   auxInitDisplayMode( AUX_DOUBLE | AUX_RGB | AUX_DEPTH );
   auxInitWindow ( "A test" );

#ifdef EXGLAUX
   auxExKeyFunc( KeyProc );   // MAZAR
   auxExMouseFunc( MouseFunc );
#else
   auxKeyFunc(AUX_ESCAPE,  EscapeKeyProc );
   auxKeyFunc(AUX_s,       SKeyProc );
   auxKeyFunc(AUX_UP,      UpKeyProc );
   auxKeyFunc(AUX_DOWN,    DownKeyProc );
   auxKeyFunc(AUX_LEFT,    LeftKeyProc );
   auxKeyFunc(AUX_RIGHT,   RightKeyProc );
#endif

   auxReshapeFunc( ReShape );

   UpdateInfo();

   auxMainLoop( MainProc );

}
