#include "engine.h"

namespace Sockets {
    bool startup() {
        WSAData wsaData;
        if(WSAStartup(MAKEWORD(2,2),&wsaData)!=0)
            return false;
        return true;
    }
    void shutdown() {
        WSACleanup();
    }
}
bool startServer(char* port, SOCKET &ListenSocket) {
    struct addrinfo *result=NULL,hints;
    ZeroMemory(&hints,sizeof(hints));
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_protocol=IPPROTO_TCP;
    hints.ai_flags=AI_PASSIVE;
    if(getaddrinfo(NULL,port,&hints,&result)!=0) {
        return false;
    }
    ListenSocket=INVALID_SOCKET;
    ListenSocket=socket(result->ai_family,result->ai_socktype,result->ai_protocol);
    if(ListenSocket==INVALID_SOCKET) {
        freeaddrinfo(result);
        return false;
    }
    if(bind(ListenSocket,result->ai_addr,(int)result->ai_addrlen)==SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(ListenSocket);
        return false;
    }
    freeaddrinfo(result);
    if(listen(ListenSocket,SOMAXCONN)==SOCKET_ERROR) {
        closesocket(ListenSocket);
        return false;
    }
    return true;
}

bool Engine::init() {
	if(!glfwInit()) {
		return false;
	}
	glfwWindowHint(GLFW_SAMPLES,16);
	glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
	//window=glfwCreateWindow(1920,1080,"Project Zero",glfwGetPrimaryMonitor(),NULL);
	window=glfwCreateWindow(1920,1080,"Project Zero",NULL,NULL);
	if(!window) {
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(window);
	glewExperimental=GL_TRUE;
	if(glewInit()!=GLEW_OK) {
		glfwTerminate();
		return false;
	}
	glEnable(GL_DEPTH_TEST);
	glGenVertexArrays(1,&VertexArrayID);
	glBindVertexArray(VertexArrayID);
	return true;
}

void Engine::server() {
	std::thread thread(&Engine::serverTCP,this);
	while(!connected && !failure) {
		Sleep(10);
	}
	if(failure) {
		thread.join();
		return;
	}
	bool quit=false;
	Map map;
	map.init("default");
	Program program;
	program.attachShader(Shader("vertex.shader",GL_VERTEX_SHADER));
	Shader fragment("fragment.shader",GL_FRAGMENT_SHADER);
	char result[1024];
	fragment.getResult(result,NULL);
	writeDebug(result);
	program.attachShader(fragment);
	program.create();
	mat4 Projection=perspective(45.0f,1920.0f/1080.0f,0.1f,1000.0f);
    mat4 Model=mat4(1.0f);
    mat4 View;
    mat4 MVP;
	Texture texture;
	texture.loadPNG("texture.png");
	while(!quit) {
		if(glfwWindowShouldClose(window)) {
			quit=true;
			break;
		}
		if(glfwGetKey(window,GLFW_KEY_ESCAPE)==GLFW_PRESS) {
			quit=true;
			break;
		}
		if(!connected) {
			quit=true;
			break;
		}
		glClearColor(0.0f,0.0f,0.0f,1.0f);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		View=lookAt(vec3(x,2.0f,z),vec3(cos(angle)*32.0f,0.0f,sin(angle)*32.0f),vec3(0,1,0));
        MVP=Projection*View*Model;
        glUniformMatrix4fv(program.getUniformLocation("MVP"),1,GL_FALSE,&MVP[0][0]);
		program.use();
		texture.use(program);
		map.render();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	texture.destroy();
	program.destroy();
	running=false;
	thread.join();
}

void Engine::serverTCP() {
	SOCKET ListenSocket = INVALID_SOCKET;
	if(!Sockets::startup()) {
		failure=true;
		return;
	}
	if(!startServer("4500",ListenSocket)) {
		Sockets::shutdown();
		failure=true;
		return;
	}
    struct sockaddr_in client_info={0};
    int addrsize=sizeof(client_info);
	SOCKET ClientSocket=INVALID_SOCKET;
	ClientSocket=accept(ListenSocket,(struct sockaddr*)&client_info,&addrsize);
	if(ClientSocket==INVALID_SOCKET) {
		closesocket(ListenSocket);
		failure=true;
		return;
	}
	int t;
	char* ip;
	ip=inet_ntoa(client_info.sin_addr);
	connected=true;
	char buffer[1024];
	while(running) {
		int size=sizeof(float);
		float temp_x=x;
		float temp_z=z;
		memcpy(buffer,&temp_x,size);
		memcpy(buffer+size,&temp_z,size);
        t=send(ClientSocket,buffer,size*2,0);
		if(t==SOCKET_ERROR) {
			failure=true;
			break;
		}
		t=recv(ClientSocket,buffer,size,0);
		if(t!=size) {
			failure=true;
			break;
		}
		float temp;
		memcpy(&temp,buffer,size);
		angle=temp;
	}
	closesocket(ClientSocket);
    closesocket(ListenSocket);
	connected=false;
	Sockets::shutdown();
}

void Engine::client(const char* ip) {
	std::thread thread(&Engine::clientTCP,this,ip);
	while(!connected && !failure) {
		Sleep(10);
	}
	if(failure) {
		thread.join();
		return;
	}
	bool quit=false;
	Map map;
	map.init("default");
	Program program;
	program.attachShader(Shader("vertex.shader",GL_VERTEX_SHADER));
	Shader fragment("fragment.shader",GL_FRAGMENT_SHADER);
	char result[1024];
	fragment.getResult(result,NULL);
	writeDebug(result);
	program.attachShader(fragment);
	program.create();
	mat4 Projection=perspective(45.0f,1920.0f/1080.0f,0.1f,1000.0f);
    mat4 Model=mat4(1.0f);
    mat4 View;
    mat4 MVP;
	Texture texture;
	texture.loadPNG("texture.png");
	while(!quit) {
		if(glfwWindowShouldClose(window)) {
			quit=true;
			break;
		}
		if(glfwGetKey(window,GLFW_KEY_ESCAPE)==GLFW_PRESS) {
			quit=true;
			break;
		}
		if(!connected) {
			quit=true;
			break;
		}
		glClearColor(0.0f,0.0f,0.0f,1.0f);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		angle=angle+0.005f;
		View=lookAt(vec3(x,2.0f,z),vec3(cos(angle)*32.0f,0.0f,sin(angle)*32.0f),vec3(0,1,0));
        MVP=Projection*View*Model;
        glUniformMatrix4fv(program.getUniformLocation("MVP"),1,GL_FALSE,&MVP[0][0]);
		program.use();
		texture.use(program);
		map.render();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	texture.destroy();
	program.destroy();
	running=false;
	thread.join();
}

void Engine::clientTCP(const char* ip) {
	if(!Sockets::startup()) {
		failure=true;
		return;
	}
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo *result=NULL,*ptr=NULL,hints;
    ZeroMemory(&hints,sizeof(hints));
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_protocol=IPPROTO_TCP;
    if(getaddrinfo(ip,"4500",&hints,&result)!=0) {
        failure=true;
        return;
    }
    ptr=result;
    ConnectSocket=socket(ptr->ai_family,ptr->ai_socktype,ptr->ai_protocol);
    if(ConnectSocket==INVALID_SOCKET) {
        failure=true;
        return;
    }
    if(connect(ConnectSocket,ptr->ai_addr,(int)(ptr->ai_addrlen))==SOCKET_ERROR) {
        failure=true;
        return;
    }
    freeaddrinfo(result);
    if(ConnectSocket==INVALID_SOCKET) {
        failure=true;
        return;
    }
	connected=true;
	char buffer[1024];
	int t;
	while(running) {
		int size = sizeof(float);
		t=recv(ConnectSocket,buffer,size*2,0);
		if(t!=size*2) {
			failure=true;
			break;
		}
		float temp_x,temp_z;
		memcpy(&temp_x,buffer,size);
		memcpy(&temp_z,buffer+size,size);
		float temp=angle;
		memcpy(buffer,&temp,size);
		t=send(ConnectSocket,buffer,size,0);
		if(t==SOCKET_ERROR) {
			failure=true;
			break;
		}
	}
	closesocket(ConnectSocket);
	connected=false;
	Sockets::shutdown();
}

void Engine::destroy() {
	glfwDestroyWindow(window);
	glDeleteVertexArrays(1,&VertexArrayID);
	glfwTerminate();
}

void Engine::writeDebug(const char* error) {
	FILE* f=fopen("debug.txt","w");
	fprintf(f,"%s\n",error);
	fclose(f);
}