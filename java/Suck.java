import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.lang.*;
import java.text.*;

public class Suck  {
    public static void main(String[] args) {
	/* first declare all our variables */
	Frame f; /* our master frame */
	Panel ctl_pan, stat_pan, errs_pan; /* this panel will always stay on the screen  */
	TextArea stats, errs; /* where we display the messages and errors */
	TextField ctl_count, ctl_speed; /* where we display count and BPS */
	Label stat_lab, err_lab, ctl_count_label, ctl_speed_label;
	Button exit_button; /* how we exit our program */
	Runtime rt; /* the runtime environment for our program */
	Process prcs; /* the actual program we run */
	InputStreamReader stdin, stderr; /* output from prg */
	BufferedReader in_br, err_br;    /* buffered output */
	Thread std_thread, err_thread;   /* threads to update TextAreas*/
	Menu Option; /* our option menu */
	MenuBar mb;  /* the menu bar */
	MenuItem menu_exit;

	/* My custom classes */
	SuckMenu menu_action;
	SuckUpdate std_upd, err_upd;    /* for our Threads */
	 
	/* sanity check our args */
	if(args.length == 0) {
	    System.out.println("No command to run, aborting\n");
	    System.exit(0);
	}

	/* set up our frame and size it */
	f = new Frame("suck");
	f.setSize(650,300);

	/* set up our panel for our status messages */
	stat_pan = new Panel();
	stats = new TextArea( 5,60 );
	stat_lab = new Label("Status Messages");
	stat_pan.add(stat_lab);	
	stat_pan.add(stats);
	    
	/* set up  our panel for our error messages */
	errs_pan = new Panel();
	errs = new TextArea( 5,60);
	err_lab = new Label("Error Messages");
	errs_pan.add(err_lab);
	errs_pan.add(errs);
	
	/* declare our panel for our controls */
	ctl_pan = new Panel();

	/* now set up our textfields for message count and BPS */
	ctl_count = new TextField("",10);
	ctl_count_label = new Label("Messages Left = ");
	ctl_speed = new TextField("",10);
	ctl_speed_label = new Label("Download Speed = ");
		
	/* make em non-editable by user */
	ctl_count.setEditable(false);
	ctl_speed.setEditable(false);

	/* add em to our frame */
	ctl_pan.add(ctl_count_label);
	ctl_pan.add(ctl_count);
	ctl_pan.add(ctl_speed_label);
	ctl_pan.add(ctl_speed);

	/* now add em to our master frame */
	f.add(ctl_pan,BorderLayout.NORTH);
	f.add(stat_pan,BorderLayout.CENTER);	
	f.add(errs_pan,BorderLayout.SOUTH);

	/* set up our menu bar */
	/* first, set up our menu item(s) */
	menu_exit = new MenuItem("Exit"); /* this is the word we display */
	menu_exit.setActionCommand("byebye"); /* this is the action */

	/* now add em to our menu */
	Option = new Menu("Options", true);
	Option.add(menu_exit);

	/* now set up our action */
	menu_action = new SuckMenu();
	Option.addActionListener(menu_action);
	
	/* now add the menu to our menu bar */
	mb = new MenuBar();
	mb.add(Option);
	f.setMenuBar(mb);

	/* all done setting up, show it */
	f.show();

	// now run our program 
	try {
	    rt = Runtime.getRuntime();
	    prcs = rt.exec(args);
	    
	    // get stdin first
	    stdin = new InputStreamReader(prcs.getInputStream());
	    in_br = new BufferedReader(stdin);
	    // now stderr
	    stderr = new InputStreamReader(prcs.getErrorStream());
	    err_br = new BufferedReader(stderr);

	    // Now set up and run our threads.
	    std_upd = new SuckUpdate();
	    err_upd = new SuckUpdate();
	
	    /* set up the stats one to parse message count and BPS */
	    std_upd.setUp(stats, in_br, ctl_count, ctl_speed);
	    /* error one doesn't parse message count and BPS */
	    err_upd.setUp(errs, err_br);

	    std_thread = new Thread(std_upd);
	    err_thread = new Thread(err_upd);

	    std_thread.start();
	    err_thread.start();

	} catch(IOException ioe) {
	    System.out.println(ioe);
	}
    }
}
// following is an class to handle the menu
class SuckMenu implements ActionListener {

    public void actionPerformed (ActionEvent e) {
	String s = e.getActionCommand();

	if("byebye".equals(s)) {
	    System.out.println("Exiting program.");
	    System.exit(0);
	}	
    }
}
// this class updates the status and message areas
class SuckUpdate implements Runnable {
    TextArea Text;   // area to update
    BufferedReader Data;  // where we get our data from
    TextField MsgCount, BPS;

    /* if only called with these two we won't parse messagecount and BPS */
    public void setUp(TextArea inp, BufferedReader br) {
	Text = inp;
	Data = br;
	MsgCount = null;
	BPS = null;
    }
    public void setUp(TextArea inp, BufferedReader br, TextField count, TextField bps) {
	Text = inp;
	Data = br;
	MsgCount = count;
	BPS = bps;
    }
    public void run() {
	String line;
	int count = 0;
	double bps = 0.0;
	Double dbl;
	int sep;
	DecimalFormat bpsf = new DecimalFormat( "#.0");

	try {
	    while((line = Data.readLine()) != null) {
		if(BPS != null && line.startsWith("---", 0)) {
		    /* handle our message count and BPS lines */
		    /* the lines are formatted as follows*/
		    /* ---message_nr+++ BPS */
		    /* BPS may not be there */
		    sep = line.indexOf("+++");
		    try {
			count = Integer.parseInt(line.substring(3,sep));
			if(sep+3 < line.length()) {
			    /* get the BPS which is a double, and there's no direct way to get to a string  */
			    dbl = Double.valueOf(line.substring(sep+3));
			    bps = dbl.doubleValue();
			}
		    } catch (NumberFormatException nfe) {
			System.out.println(line + ":" + nfe);
			System.out.println("sep = " + sep + " count =" + count + " bps=" +bps);
		    }
		    MsgCount.setText(String.valueOf(count));
		    BPS.setText(bpsf.format(bps) + " BPS");
		}
		else {
		    Text.append(line);
		    Text.append("\n");
		}
	    }
	    Data.close();
	}
	catch (IOException ioe) {
	    System.out.println(ioe);
	}
    }
}

