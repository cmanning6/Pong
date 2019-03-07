/*
 * Pong Framework with a basic Seven Segment Display (SSD)
 * Author: Randi A.
 * Modified By: Chad Manning
 * Course: CMPS 3350
 * Date: March3rd, 2019
 * To Compile : g++ pong.cpp -Wall -lrt -lX11 -lGL -o pong
 */
#include <bitset>   //bit string library
#include <string.h> //memset
#include <iostream>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include <math.h> 

using namespace std;

class pWindow {
private:
	Display *dpy;
	Window win;
	GLXContext glc;
public:
	pWindow() {
		GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
		XSetWindowAttributes swa;
		dpy = XOpenDisplay(NULL);
		if (dpy == NULL) {
			std::cout << "\n\tcannot connect to X server" << std::endl;
			exit(EXIT_FAILURE);
		}
		Window root = DefaultRootWindow(dpy);
		XVisualInfo *vi = glXChooseVisual(dpy, 0, att);
		if (vi == NULL) {
			std::cout << "\n\tno appropriate visual found\n" << std::endl;
			exit(EXIT_FAILURE);
		} 
		Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
		swa.colormap = cmap;
		swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
			PointerMotionMask | MotionNotify | ButtonPress | ButtonReleaseMask |
			StructureNotifyMask | SubstructureNotifyMask;
		win = XCreateWindow(dpy, root, 0, 0, 960, 720, 0,
				vi->depth, InputOutput, vi->visual,
				CWColormap | CWEventMask, &swa);
		set_title();
		glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
		glXMakeCurrent(dpy, win, glc);
	}
	~pWindow() {
		XDestroyWindow(dpy, win);
		XCloseDisplay(dpy);
	}
	void set_title() {
		XMapWindow(dpy, win);
		XStoreName(dpy, win, "Pong Lab - Introduction to Basic Game Physics");
	}
	void swapBuffers() {
		glXSwapBuffers(dpy, win);
	}
	bool getXPending() {
		return XPending(dpy);
	}
	XEvent getXNextEvent() {
		XEvent e;
		XNextEvent(dpy, &e);
		return e;
	}
};

struct Global {
    char keys[65536];
    int p1Score, p2Score;
    Global() {
        memset(keys, 0, 65536);
        p1Score = p2Score = 0;
    }
} G;

/* Notice the G after the closing curly brace and the semicolon in the Global
 * struct? What we're doing is declaring an instance of the Global struct as
 * global. It would similar if we declared it as "Global G;" outside and before
 * main().
 */

struct Bumper {
    float position[2];
    float velocity; //Needed to add drift
    float color[3] = {1.0f, 1.0f, 1.0f};
    
    Bumper(int x, int y) {
        position[0] = x;
        position[1] = y;
        velocity = 0;
    }
    void setPos(float x, float y) {
        position[0] = x;
        position[1] = y;
    }
    void drawBumper() {
        if (color[0] < 1.0f) {
            color[0] = color[2] += 0.01f;
        }
        glColor3f(color[0], color[1], color[2]);
        glBegin(GL_POLYGON);
            glVertex2f(0.0f, 63.0f);
            glVertex2f(9.0f, 63.0f);
            glVertex2f(9.0f, 0.0f);
            glVertex2f(0.0f, 0.0f);
        glEnd();
    }
} rightBumper(933, 360), leftBumper(27, 360);

struct Ball {
    float pos[2];
    float vel[2];
    float color[3] {1.0f, 1.0f, 1.0f};
    const float MAX_SPEED = 8.0;

    Ball() {
        resetBall();
    }
    void drawBall(){
        const float PI = 3.14;
        int triangleAmount = 20; //# of triangles used to draw circle
        
        GLfloat radius = 5.0f; //radius
        GLfloat twicePi = 2.0f * PI;
        
        glColor3f(color[0], color[1], color[2]);
        glBegin(GL_TRIANGLE_FAN);
            for(int i = 0; i <= triangleAmount;i++) { 
                glVertex2f(
                    (radius * cos(i *  twicePi / triangleAmount)), 
                    (radius * sin(i * twicePi / triangleAmount))
                );
            }
        glEnd();
    }
    void resetBall() {
        srand(time(0));

        vel[0] = 2;
        vel[1] = rand() % 2 + 1;
        if (rand()%100 < 49)
            vel[1] *= -1;

        pos[0] = 480;
        pos[1] = 360;
        color[1] = color[2] = 1.0f;
    }
    
} ball[3];

struct Barrier {
    float endPoint1[2];
    float endPoint2[2];
    Barrier() {
        endPoint1[0] = endPoint2[0] = 480.0f;
        endPoint1[1] = 0.0f;
        endPoint2[1] = 720.0f;
    }
    void drawBarrier() {
        //quick and dirty way of drawing a dashed line, do not recommand
        glColor3f(1.0f, 1.0f, 1.0f);
        glLineWidth(12.0f);
        glPushAttrib(GL_ENABLE_BIT); 
            glLineStipple(9, 0x9999);
            glEnable(GL_LINE_STIPPLE);
            glBegin(GL_LINES);
                glVertex2f(endPoint1[0], endPoint1[1]);
                glVertex2f(endPoint2[0], endPoint2[1]);
            glEnd();
        glPopAttrib();
    }
} barrier;

struct SSD { //Seven Segment Display (SSD)
    //abcdefg - #
    //1111110 - 0
    //0110000 - 1
    //1101101 - 2
    //1111001 - 3
    //0110011 - 4
    //1011011 - 5
    //0011111 - 6
    //1110000 - 7
    //1111111 - 8
    //1110011 - 9
    bitset<7> screen;

    SSD() {
        screen = bitset<7>("1111110"); // displaying 0
    }
    //update the display, score > 9 will print display "E" for error
    void updateDisplay(int score) {
        switch(score) {
            case 0:
                screen = bitset<7>("1111110");
                break;
            case 1:
                screen = bitset<7>("0110000");
                break;
            case 2:
                screen = bitset<7>("1101101");
                break;
            case 3:
                screen = bitset<7>("1111001");
                break;
            case 4:
                screen = bitset<7>("0110011");
                break;
            case 5:
                screen = bitset<7>("1011011");
                break;
            case 6:
                screen = bitset<7>("0011111");
                break;
            case 7:
                screen = bitset<7>("1110000");
                break;
            case 8:
                screen = bitset<7>("1111111");
                break;
            case 9:
                screen = bitset<7>("1110011");
                break;
            default:
                screen = bitset<7>("1001111");
                break;
        }
    }
    void renderSSD() {
        if (screen[6]) { //a
            glBegin(GL_POLYGON);
                glVertex2f(0.0f, 100.0f);
                glVertex2f(0.0f, 110.0f);
                glVertex2f(40.0f, 110.0f);
                glVertex2f(40.0f, 100.0f);
            glEnd();
        }
        if (screen[5]) { //b
            glBegin(GL_POLYGON);
                glVertex2f(30.0f, 50.0f);
                glVertex2f(30.0f, 110.0f);
                glVertex2f(40.0f, 110.0f);
                glVertex2f(40.0f, 50.0f);
            glEnd();
        }
        if (screen[4]) { //c
            glBegin(GL_POLYGON);
                glVertex2f(30.0f, 0.0f);
                glVertex2f(30.0f, 50.0f);
                glVertex2f(40.0f, 50.0f);
                glVertex2f(40.0f, 0.0f);
            glEnd();
        }
        if (screen[3]) { //d
            glBegin(GL_POLYGON);
                glVertex2f(0.0f, 0.0f);
                glVertex2f(0.0f, 10.0f);
                glVertex2f(40.0f, 10.0f);
                glVertex2f(40.0f, 0.0f);
            glEnd();
        }
        if (screen[2]) { //e
            glBegin(GL_POLYGON);
                glVertex2f(0.0f, 0.0f);
                glVertex2f(0.0f, 50.0f);
                glVertex2f(10.0f, 50.0f);
                glVertex2f(10.0f, 0.0f);
            glEnd();
        }
        if (screen[1]) { //f
            glBegin(GL_POLYGON);
                glVertex2f(0.0f, 50.0f);
                glVertex2f(0.0f, 110.0f);
                glVertex2f(10.0f, 110.0f);
                glVertex2f(10.0f, 50.0f);
            glEnd();
        }

        if (screen[0]) { //g       
            glBegin(GL_POLYGON);
                glVertex2f(0.0f, 50.0f);
                glVertex2f(0.0f, 60.0f);
                glVertex2f(40.0f, 60.0f);
                glVertex2f(40.0f, 50.0f);
            glEnd();
        }
    }
} ssd1, ssd2, ssd3, ssd4;

void initialize_OpenGL();
void check_key(XEvent *e);
void physics();
void render();

int main() 
{
	//play window
	pWindow pWin; //window instance
    initialize_OpenGL();
	while(1) {
        while (pWin.getXPending()) {
            XEvent e = pWin.getXNextEvent();
            check_key(&e);
        }
        physics();
        render();
        pWin.swapBuffers();

	}
	return 0;
}

void initialize_OpenGL() {
    glViewport(0, 0, 960, 720);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glOrtho(0, 960, 0, 720, -1, 1);

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); //background color
}

void check_key(XEvent *e) {
    int key = (XLookupKeysym(&e->xkey, 0) & 0x0000ffff);
    // if key is relased set to 0
    if (e->type == KeyRelease) {
        G.keys[key] = 0;
    }
    // if key is pressed set to 1
    if (e->type == KeyPress) {
        G.keys[key] = 1;
    }
}

void physics() {
    //start the ball moving
    const float VEL_CHANGE = 2.0f;
    const float DECEL = 1.0f;
    ball[0].pos[0] += ball[0].vel[0];
    ball[0].pos[1] += ball[0].vel[1];
    //Determine where trailing balls will be
    if (ball[0].vel[0] > 0) {
        ball[1].pos[0] = ball[0].pos[0] + (ball[0].vel[0] * -2); 
        ball[1].pos[1] = ball[0].pos[1] - (ball[0].vel[1] * 2);  
        ball[2].pos[0] = ball[0].pos[0] + (ball[0].vel[0] * -4);  
        ball[2].pos[1] = ball[0].pos[1] - (ball[0].vel[1] * 4);  
    } else {
        ball[1].pos[0] = ball[0].pos[0] + (ball[0].vel[0] * -2); 
        ball[1].pos[1] = ball[0].pos[1] - ball[0].vel[1];
        ball[2].pos[0] = ball[0].pos[0] + (ball[0].vel[0] * -4); 
        ball[2].pos[1] = ball[0].pos[1] - ball[0].vel[1] * 2;
    }
    leftBumper.position[1] += leftBumper.velocity;
    if (leftBumper.velocity > 0)
        leftBumper.velocity -= DECEL;
    if (leftBumper.velocity < 0)
        leftBumper.velocity += DECEL;
    rightBumper.position[1] += rightBumper.velocity;
    if (rightBumper.velocity > 0)
        rightBumper.velocity -= DECEL;
    if (rightBumper.velocity < 0)
        rightBumper.velocity += DECEL;

    //keys w and s for left bumper
    if (G.keys[XK_w]) {
        leftBumper.velocity += VEL_CHANGE;
    }
    if (G.keys[XK_s]) {
        leftBumper.velocity -= VEL_CHANGE;
    }
    //keys o and l for right bumper
    if (G.keys[XK_o]) {
        rightBumper.velocity += VEL_CHANGE;
    }
    if (G.keys[XK_l]) {
        rightBumper.velocity -= VEL_CHANGE;
    }
    //collision with top and bottom of game window
    if ((ball[0].pos[1] > 715) ) {
        ball[0].vel[1] *= -1.0f;
        ball[0].pos[1] = 715;
    } else if (ball[0].pos[1] < 5) {
        ball[0].vel[1] *= -1.0f;
        ball[0].pos[1] = 5;
    }
    //p2 scores
    if (ball[0].pos[0] < 0) {
        ball[0].resetBall();
        G.p2Score++;
        ssd3.updateDisplay(G.p2Score / 10);
        ssd4.updateDisplay(G.p2Score % 10);
    }
    //p1 scores
    if (ball[0].pos[0] > 960) {
        ball[0].resetBall();
        G.p1Score++;
        ssd1.updateDisplay(G.p1Score / 10);
        ssd2.updateDisplay(G.p1Score % 10);
    }
    //collisions with bumpers, you need to make sure you reset the the ball[1]'s 
    //position to either left or right of a bumper, so it won't get stuck if it's going to fast

    //left bumper
    if ((ball[0].pos[1] <= (leftBumper.position[1] + 63.0f)) && //top y
        (ball[0].pos[1] >= (leftBumper.position[1])) && //bottom y
        (ball[0].pos[0] >= (leftBumper.position[0])) && //left x
        (ball[0].pos[0] <= (leftBumper.position[0] + 9.0f))) { //right x  {
            leftBumper.color[0] = leftBumper.color[2] = 0.0f;
            ball[0].pos[0] = leftBumper.position[0] + 9.0f;
            if(ball[0].vel[0]*-1 < ball[0].MAX_SPEED) {
                ball[0].vel[0] *= -1.2f;
			} else {
				ball[0].vel[0] *= -1;
			}
			cout << ball[0].vel[0] << endl;
            ball[0].color[1] = ball[0].color[2] -= 0.1f;
    }
        //right bumper
    if ((ball[0].pos[1] <= (rightBumper.position[1] + 63.0f)) && //top y
        (ball[0].pos[1] >= (rightBumper.position[1])) && //bottom y
        (ball[0].pos[0] + 5.0f >= (rightBumper.position[0])) && //left x
        (ball[0].pos[0] + 5.0f <= (rightBumper.position[0] + 9.0f))) {
        //negation of the ball[1]'s x-velocity
            rightBumper.color[0] = rightBumper.color[2] = 0.0f;
            if(ball[0].vel[0] < ball[0].MAX_SPEED) {
                ball[0].vel[0] *= -1.2f;
			} else {
				ball[0].vel[0] *= -1;
			}
			cout << ball[0].vel[0] << endl;
            ball[0].pos[0] = rightBumper.position[0]-9.0f;
            ball[0].color[1] = ball[0].color[2] -= 0.1f;
    }
}

void render() {
    //clear buffer so we don't get any weird artifacts
    glClear(GL_COLOR_BUFFER_BIT); 
    //draw left bumper
    if (leftBumper.position[1] + 63 > 720)
        leftBumper.position[1] = 720 - 63;
    if (leftBumper.position[1] < 0)
        leftBumper.position[1] = 0;
    glPushMatrix();
    glTranslated(leftBumper.position[0], leftBumper.position[1], 0);
    leftBumper.drawBumper();
    glPopMatrix();
    //draw right bumper
    if (rightBumper.position[1] + 63 > 720)
        rightBumper.position[1] = 720 - 63;
    if (rightBumper.position[1] < 0)
        rightBumper.position[1] = 0;
    glPushMatrix();
    glTranslated(rightBumper.position[0], rightBumper.position[1], 0);
    rightBumper.drawBumper();
    glPopMatrix();
    //draw ball[2]
    glPushMatrix();
    glTranslated(ball[2].pos[0], ball[2].pos[1], 1);
    ball[2].color[0] = ball[2].color[1] = ball[2].color[2] = 0.3f;
    ball[2].drawBall();
    glPopMatrix();
    //draw ball[1]
    glPushMatrix();
    glTranslated(ball[1].pos[0], ball[1].pos[1], 1);
    ball[1].color[0] = ball[1].color[1] = ball[1].color[2] = 0.6f;
    ball[1].drawBall();
    glPopMatrix();
    //draw ball[0]
    glPushMatrix();
    glTranslated(ball[0].pos[0], ball[0].pos[1], 0.0);
    ball[0].drawBall();
    glPopMatrix();
    //draw barrier
    glPushMatrix();
    barrier.drawBarrier();
    glPopMatrix();
    //draw p1 ssd
    glPushMatrix();
    glTranslated(230.0f, 600.0f, 0.0f);
    ssd1.renderSSD();
    glPopMatrix();
    //draw p1 ssd
    glPushMatrix();
    glTranslated(280.0f, 600.0f, 0.0f);
    ssd2.renderSSD();
    glPopMatrix();
    //draw p2 ssd
    glPushMatrix();
    glTranslated(630.0f, 600.0f, 0.0f);
    ssd3.renderSSD();
    glPopMatrix();
    //draw p2 ssd
    glPushMatrix();
    glTranslated(680.0f, 600.0f, 0.0f);
    ssd4.renderSSD();
    glPopMatrix();
}
