#include <X11/Xlib.h>

void winToClient(Window win) // the client creator. packages the window being passed into a client
{

	//Increment the amount of clients 
	numClients++;

	//Increase the size of the array
	clients = realloc(clients, sizeof(Client) * numClients);

	//If allocation fails
	if(clients == NULL){
		fprintf(logFile, "%ld: FAILED TO ALLOCATE ENOUGH MEMORY TO THE CLIENT ARRAY\nPANIC QUITTING\n", clock());
		free(clients);
		exit(1);
	}

	//Get the attributes for the window
	XWindowAttributes winAt;
	XGetWindowAttributes(dpy, win, &winAt);
	clients[numClients - 1].dpy = dpy;
	clients[numClients - 1].parent = &root;
	clients[numClients - 1].win = win;
	clients[numClients - 1].coords[0] = winAt.x;
	clients[numClients - 1].coords[1] = winAt.y;
	clients[numClients - 1].dimensions[0] = winAt.width;
	clients[numClients - 1].dimensions[1] = winAt.height;
	clients[numClients - 1].borderColor = wmParams[COLOR_INACTIVE];
	clients[numClients - 1].indPrev = -1;
	clients[numClients - 1].isMini = 0;

	XTextProperty winName;
	XGetWMName(dpy, win, &winName);
	fprintf(logFile, "%ld: CREATED NEW CLIENT #%ld NAMED: %s\n", clock(), numClients, winName.value);


	drawBorders(numClients - 1, wmParams[THICKNESS_INACTIVE]);
}

long unsigned int checkClient(Window win) // checkes what client is being passed and returns its index in the master list
{
	long unsigned int i;

	for(i = 0; i < numClients; i++)
		{
			if(clients[i].win == win)
			{
return i;
			}
	}

	winToClient(win);
	return numClients - 1;
}

void focusWin(Window win) // changes the focused window
{
	long unsigned int i = checkClient(win);

	if(lastFocusedClient != i) // resetting the borders of the last focused window
	{
		clients[i].indPrev = lastFocusedClient;	// setting the current client as the last focused for the next execution of this function
		clients[clients[i].indPrev].borderColor = wmParams[COLOR_INACTIVE];

		drawBorders(clients[i].indPrev, wmParams[THICKNESS_INACTIVE]);
		fprintf(logFile, "%ld: FOCUS SWITCHED FROM CLIENT #%ld TO CLIENT #%ld\n", clock(), clients[i].indPrev + 1, i + 1);
	}


	//setting the borders of the current window
	clients[i].borderColor = wmParams[COLOR_ACTIVE];
	XSetInputFocus(dpy, clients[i].win, RevertToParent, CurrentTime);
	XRaiseWindow(dpy, clients[i].win);
	lastFocusedClient = i;

	drawBorders(i, wmParams[THICKNESS_ACTIVE]);

}

void killWin(Window win) //kills the client
{
	long unsigned int i = checkClient(win);
	XEvent evnt;

	// This was weird to figure out at first but basically it sends the current window a message to politely shut off instead of being forceful and using something like XDestroyWindow()

	evnt.type = ClientMessage;
	evnt.xclient.window = clients[i].win;
	evnt.xclient.message_type = XInternAtom(dpy, "WM_PROTOCOLS", True);
	evnt.xclient.format = 32;
	evnt.xclient.data.l[0] = XInternAtom(dpy, "WM_DELETE_WINDOW", True);
	evnt.xclient.data.l[1] = CurrentTime;

	if(clients[i].indPrev != -1)
	{
		focusWin(clients[clients[i].indPrev].win);
	}
	if(clients[i].isMini)
	{
		if(numMini != 0)
		{
			numMini--;
		}
	}

	XSendEvent(dpy, clients[i].win, False, NoEventMask, &evnt);

	fprintf(logFile, "%ld: DELETING CLIENT #%ld FROM THE LIST\n", clock(), i + 1);

	if(numClients > 1){
		numClients--;
	}

	clients = realloc(clients, sizeof(Client) * numClients);

	if(clients == NULL){
		fprintf(logFile, "%ld: FAILED TO ALLOCATE ENOUGH MEMORY TO THE CLIENT ARRAY\nPANIC QUITTING\n", clock());
		free(clients);
		exit(1);
	}

}

void restoreWin(Window win)
{
	long unsigned int i = checkClient(win); // you should get the jist of what im doing with these by now. if not scroll back up in the code and read the explanation on previous time i use these next two functions

	//focuses the window and restores it to the last size saved in the clients attributes

	focusWin(clients[i].win);
	XResizeWindow(dpy, win, clients[i].dimensions[0], clients[i].dimensions[1]);
	XMoveWindow(dpy, win, clients[i].coords[0], clients[i].coords[1]);
	numMini--;
	if(numMini != 0)
	{
		numMini--;
	}
	clients[i].isMini = 0;
}

void maximizeWin(Window win)
{
	long unsigned int i = checkClient(win); // look at the function above

	focusWin(clients[i].win);

	//saving the unmodified dimensions of the client

	clients[i].dimensions[0] = attr.width;
	clients[i].dimensions[1] = attr.height;
	clients[i].coords[0] = attr.x;
	clients[i].coords[1] = attr.y;

	XResizeWindow(dpy, win, 1920, 1080); // this is a lazy solution that i plan on making better in the future
	XMoveWindow(dpy, win, 0, 0);

	if(clients[i].isMini)
	{
		clients[i].isMini = 0;
		if(numMini != 0)
		{
			numMini--;
		}
	}

}

void  minimizeWin(Window win)
{

	long unsigned int i = checkClient(win); // again look above


	//saving the attributes of the current client
	clients[i].dimensions[0] = attr.width;
	clients[i].dimensions[1] = attr.height;
	clients[i].coords[0] = attr.x;
	clients[i].coords[1] = attr.y;

	//lowering the window and iconifying it. in the future this will be an actual icon but for now its just a shitty 15x15 box

	XLowerWindow(dpy, clients[i].win);
	XResizeWindow(dpy, win, 15, 15);
	XMoveWindow(dpy, win, 20 * (numMini), (XDisplayHeight(dpy, scr) - 20)); // increments the icon along the bottom based on its position in numClients
	numMini++;
	clients[i].isMini = 1;
}

//And heres the big boy
int main(int argc, char **argv){ 

	//Make sure that start.subwindow is first set to None just in case
	start.xbutton.subwindow = None;
	start.xkey.subwindow = None;

	while(1)
		{
			//Grabs next event
			XNextEvent(dpy, &ev);

			//Checks if a window has just been mapped, scans the available clients to check if it
			if(ev.type == CreateNotify)
			{
				//Create a new client for the window created
				fprintf(logFile, "%ld: CLIENT CREATION DETECTED\n", clock());
				winToClient(ev.xcreatewindow.window);
				//This breaking shit so I removed it for now
				//moveClientToPointer(ev.xcreatewindow.window, ev.xbutton.x_root, ev.xbutton.y_root);
			}
			//Checking whether a button was pressed and also if theres a window that exists where the button was pressed
			else if((ev.type == ButtonPress && ev.xbutton.subwindow != None) || (ev.type == KeyPress && ev.xkey.subwindow != None))
			{

				//Save a copy of the current event. This will be used later for things like moving and resizing windows
				start = ev;

				//Assigns attributes based whether a key or mouse button was pressed
				if(start.type == ButtonPress){
					fprintf(logFile, "%ld: BUTTON PRESS (%d) DETECTED\n", clock(), start.xbutton.button);
					XGetWindowAttributes(dpy, start.xbutton.subwindow, &attr);
					XSetInputFocus(dpy, start.xbutton.subwindow, RevertToParent, CurrentTime);
				}else if(start.type == KeyPress){
					fprintf(logFile, "%ld: KEY PRESS (%c) DETECTED\n", clock(), (char)start.xkey.keycode);
					XGetWindowAttributes(dpy, start.xkey.subwindow, &attr);
					XSetInputFocus(dpy, start.xkey.subwindow, RevertToParent, CurrentTime);
				}

				if(start.type == ButtonPress ? (start.xbutton.button == key[KEY_FIELD][KILL_KEY] && start.xbutton.state == key[MOD_FIELD][KILL_KEY]) : (start.xkey.keycode == XKeysymToKeycode(dpy, key[KEY_FIELD][KILL_KEY]) && start.xkey.state == key[MOD_FIELD][KILL_KEY]))
				{
					fprintf(logFile, "%ld: KILL WINDOW CALLED\n", clock());
					//Kills the selected window
					killWin(start.type == ButtonPress ? start.xbutton.subwindow : start.xkey.subwindow);
					continue;
				}
				else if(start.type == ButtonPress ? (start.xbutton.button == key[KEY_FIELD][FOCUS_KEY] && start.xbutton.state == key[MOD_FIELD][FOCUS_KEY]) : start.xkey.keycode == XKeysymToKeycode(dpy, key[KEY_FIELD][FOCUS_KEY]) && start.xkey.state == key[MOD_FIELD][FOCUS_KEY])
				{
					fprintf(logFile, "%ld: FOCUS WINDOW CALLED\n", clock());
					//Raises and focuses the window
					focusWin(start.type == ButtonPress ? start.xbutton.subwindow : start.xkey.subwindow);
					continue;
				}
				else if(start.type == ButtonPress ? (start.xbutton.button == key[KEY_FIELD][RESTO_KEY] && start.xbutton.state == key[MOD_FIELD][RESTO_KEY]) : (start.xkey.keycode == XKeysymToKeycode(dpy, key[KEY_FIELD][RESTO_KEY]) && start.xkey.state == key[MOD_FIELD][RESTO_KEY]))
				{
					fprintf(logFile, "%ld: RESTORE WINDOW CALLED\n", clock());
					//Checks if the window is already minimized
					if(attr.height == 15 && attr.width == 15)
					{
						//Restores the window to its previous size
						restoreWin(start.type == ButtonPress ? start.xbutton.subwindow : start.xkey.subwindow);
					}
					continue;
				}
				else if(start.type == ButtonPress ? (start.xbutton.button == key[KEY_FIELD][MINI_KEY] && start.xbutton.state == key[MOD_FIELD][MINI_KEY]) : (start.xkey.keycode == XKeysymToKeycode(dpy, key[KEY_FIELD][MINI_KEY]) && start.xkey.state == key[MOD_FIELD][MINI_KEY]))
				{
					fprintf(logFile, "%ld: MINIMIZE WINDOW CALLED\n", clock());
					//Checks if the window is not minimized
					if(attr.height != 15 && attr.width != 15)
					{
						//Minimizes the window
						minimizeWin(start.type == ButtonPress ? start.xbutton.subwindow : start.xkey.subwindow);
					}
					continue;
				}
				else if(start.type == ButtonPress ? (start.xbutton.button == key[KEY_FIELD][EXIT_KEY] && start.xbutton.state == key[KEY_FIELD][EXIT_KEY]) : (start.xkey.keycode == XKeysymToKeycode(dpy, key[KEY_FIELD][EXIT_KEY]) && start.xkey.state == key[MOD_FIELD][EXIT_KEY]))
				{
					fprintf(logFile, " %ld: KILL WM CALLED\n", clock());
					//Kills the WM
					break;
				}
			}
			else if(ev.type == MotionNotify && start.xbutton.subwindow != None && start.xbutton.state == key[MOD_FIELD][MOVE_KEY]) //VERY TEMPORARY IMPLEMENTATION
			{
				if(start.type == ButtonPress && (start.xbutton.button == key[KEY_FIELD][MOVE_KEY] || start.xbutton.button == key[KEY_FIELD][SIZE_KEY])){
					int xDiff = ev.xbutton.x_root - start.xbutton.x_root;
					int yDiff = ev.xbutton.y_root - start.xbutton.y_root;

					//Long ass complicated function to move or resize windows
					XMoveResizeWindow(dpy, start.xbutton.subwindow, attr.x + (start.xbutton.button == key[KEY_FIELD][MOVE_KEY] ? xDiff : 0), attr.y + (start.xbutton.button == key[KEY_FIELD][MOVE_KEY] ? yDiff : 0), MAX(1, attr.width + (start.xbutton.button == key[KEY_FIELD][SIZE_KEY] ? xDiff : 0)), MAX(1, attr.height + (start.xbutton.button == key[KEY_FIELD][SIZE_KEY] ? yDiff : 0)));
					focusWin(ev.xbutton.subwindow);
				}
				continue;
			}
			//If the button or key is released
			else if(ev.type == ButtonRelease || ev.type == KeyRelease)
			{
				//Set the subwindow to None
				(ev.type == ButtonRelease ? start.xbutton.subwindow = None : start.xkey.subwindow);
				continue;
			}
			else if(ev.type == DestroyNotify)
			{
				//Remove the client of the window that was destroyed
				fprintf(logFile, "%ld: CLIENT DELETION DETECTED\n", clock());
				killWin(ev.xdestroywindow.window);
				continue;
			}
	}
}

/*

Kill window
Raise the window
Restore & Minimize
Kill WM

*/

        if(start.type == ButtonPress ? (start.xbutton.button == key[KEY_FIELD][KILL_KEY] && start.xbutton.state == key[MOD_FIELD][KILL_KEY]) : (start.xkey.keycode == XKeysymToKeycode(dpy, key[KEY_FIELD][KILL_KEY]) && start.xkey.state == key[MOD_FIELD][KILL_KEY]))
				{
					fprintf(logFile, "%ld: KILL WINDOW CALLED\n", clock());
					//Kills the selected window
					killWin(start.type == ButtonPress ? start.xbutton.subwindow : start.xkey.subwindow);
					continue;
				}
				else if(start.type == ButtonPress ? (start.xbutton.button == key[KEY_FIELD][FOCUS_KEY] && start.xbutton.state == key[MOD_FIELD][FOCUS_KEY]) : start.xkey.keycode == XKeysymToKeycode(dpy, key[KEY_FIELD][FOCUS_KEY]) && start.xkey.state == key[MOD_FIELD][FOCUS_KEY])
				{
					fprintf(logFile, "%ld: FOCUS WINDOW CALLED\n", clock());
					//Raises and focuses the window
					focusWin(start.type == ButtonPress ? start.xbutton.subwindow : start.xkey.subwindow);
					continue;
				}
				else if(start.type == ButtonPress ? (start.xbutton.button == key[KEY_FIELD][RESTO_KEY] && start.xbutton.state == key[MOD_FIELD][RESTO_KEY]) : (start.xkey.keycode == XKeysymToKeycode(dpy, key[KEY_FIELD][RESTO_KEY]) && start.xkey.state == key[MOD_FIELD][RESTO_KEY]))
				{
					fprintf(logFile, "%ld: RESTORE WINDOW CALLED\n", clock());
					//Checks if the window is already minimized
					if(attr.height == 15 && attr.width == 15)
					{
						//Restores the window to its previous size
						restoreWin(start.type == ButtonPress ? start.xbutton.subwindow : start.xkey.subwindow);
					}
					continue;
				}
				else if(start.type == ButtonPress ? (start.xbutton.button == key[KEY_FIELD][MINI_KEY] && start.xbutton.state == key[MOD_FIELD][MINI_KEY]) : (start.xkey.keycode == XKeysymToKeycode(dpy, key[KEY_FIELD][MINI_KEY]) && start.xkey.state == key[MOD_FIELD][MINI_KEY]))
				{
					fprintf(logFile, "%ld: MINIMIZE WINDOW CALLED\n", clock());
					//Checks if the window is not minimized
					if(attr.height != 15 && attr.width != 15)
					{
						//Minimizes the window
						minimizeWin(start.type == ButtonPress ? start.xbutton.subwindow : start.xkey.subwindow);
					}
					continue;
				}
				else if(start.type == ButtonPress ? (start.xbutton.button == key[KEY_FIELD][EXIT_KEY] && start.xbutton.state == key[KEY_FIELD][EXIT_KEY]) : (start.xkey.keycode == XKeysymToKeycode(dpy, key[KEY_FIELD][EXIT_KEY]) && start.xkey.state == key[MOD_FIELD][EXIT_KEY]))
				{
					fprintf(logFile, " %ld: KILL WM CALLED\n", clock());
					//Kills the WM
					break;
