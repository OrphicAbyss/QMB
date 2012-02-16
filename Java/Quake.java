import java.io.*;

public class Quake{
	public static void main(String args[]){
		print(CONPRINT, "Quake Java 0.1\n");
		try {
			System.setErr(new PrintStream(new FileOutputStream("jvm_err.txt")));
			System.setOut(new PrintStream(new FileOutputStream("jvm_out.txt")));
		}catch (Exception e){
			e.printStackTrace();
		}
		
		print(CONPRINT, "Testing prints - this is a console print\n");
		print(DBGPRINT, "Testing prints - this is a debug console print\n");
		print(BPRINT, "Testing prints - this is a bprint\n");
		print(SPRINT, "Testing prints - this is a sprint\n", 0);
		print(CENTERPRINT, "Testing prints - this is a center print\n", 0);
		
		print(CONPRINT, "Testing cvar, toggleing r_outline\n");
		int value = (int)getCVar("r_outline");
		if (value == 0)
			value = 1;
		else
			value = 0;
		
		setCVar("r_outline",""+value);
	}
	
	public static final int BPRINT = 0;	//print to everyone
	public static final int SPRINT = 1;	//print to a client
	public static final int CENTERPRINT = 2;//centerprint to a client
	public static final int CONPRINT = 3;	//console print to server
	public static final int DBGPRINT = 4;	//console debug print to server
	
	public static void print(int type, String str){
		switch (type){
		case SPRINT:
		case CENTERPRINT:
			error("Print type expects client.\n");
			break;
		case BPRINT:
			bPrint(str);
			break;
		case CONPRINT:
			conPrint(str);
			break;
		case DBGPRINT:
			condPrint(str);
			break;
		default:
			error("Invalid print type for message '" + str + "'\n");
		}
	}

	public static void print(int type, String str, int client){
		switch (type){
		case SPRINT:
			sPrint(str, client);
			break;
		case CENTERPRINT:
			cPrint(str, client);
			break;
		case BPRINT:
			bPrint(str);
			error("Print type does not require client\n");
			break;
		case CONPRINT:
			conPrint(str);
			error("Print type does not require client\n");
			break;
		case DBGPRINT:
			condPrint(str);
			error("Print type does not require client\n");
			break;
		default:
			error("Invalid print type for message '" + str + "'\n");
		}
	}
	
	public static void error(String str){
		conPrint("Java Quake error: " + str);
	}
	
	private static native void bPrint(String str);//bprint
	private static native void sPrint(String str, int client);//sprint
	private static native void cPrint(String str, int client);//centerprint
	private static native void conPrint(String str);//console print
	private static native void condPrint(String str);//console debug print
	
	private static native float getCVar(String cvarName);//get a cvars value
	private static native void setCVar(String cvarName, String value);//set a cvars value
	
	private static native void glTexCoord2f(float s, float t);//set a tex coord
	private static native void glVertex3f(float x, float y, float z);//set a vertex
	public static native void drawParticle(float x, float y, float z, float size, float r, float g, float b, float a, float rotation);
}