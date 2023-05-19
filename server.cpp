#include <queue>
#include <semaphore> //c++20 required
#include <mutex>
#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
struct multiqueue:public std::queue<std::string>{
	std::counting_semaphore<1024> sem;
	std::mutex ex;
	multiqueue():std::queue<std::string>(),sem(0),ex(){}
	void push(const std::string& x){
		ex.lock();
		std::queue<std::string>::push(x);
		if(size()>=1024){
			std::cout<<"queue overflow"<<std::endl;
			exit(1);
		}
		ex.unlock();
		sem.release();
	}
	std::string pop(){
		sem.acquire();
		ex.lock();
		std::string res=front();
		std::queue<std::string>::pop();
		ex.unlock();
		return res;
	}
};

void usage() {
	std::cout<<"syntax: echo-server <port> [-e[-b]]\n"<<std::endl;
	std::cout<<"  -e : echo"<<std::endl;
	std::cout<<"  -b : broadcast"<<std::endl;
}

struct Param {
	bool echo{false};
	bool broadcast{false};
	uint16_t port{0};

	bool parse(int argc, char* argv[]) {
		for (int i = 1; i < argc; i++) {
			if (argv[i]==std::string("-e")) {
				echo = true;
				continue;
			}
			if (argv[i]==std::string("-b")) {
				broadcast = true;
				continue;
			}
			port = atoi(argv[i++]);
		}
		return port != 0;
	}
} param;

void recvThread(int sd) {
	printf("connected\n");
	static const int BUFSIZE = 65536;
	char buf[BUFSIZE];
	while (true) {
		ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
		if (res == 0 || res == -1) {
			fprintf(stderr, "recv return %ld", res);
			perror(" ");
			break;
		}
		buf[res] = '\0';
		printf("%s", buf);
		fflush(stdout);
		if (param.echo) {
			res = ::send(sd, buf, res, 0);
			if (res == 0 || res == -1) {
				fprintf(stderr, "send return %ld", res);
				perror(" ");
				break;
			}
		}
	}
	printf("disconnected\n");
	::close(sd);
}

int main(int argc, char* argv[]) {
	if (!param.parse(argc, argv)) {
		usage();
		return -1;
	}

	int sd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		perror("socket");
		return -1;
	}

	int res;
	int optval = 1;
	res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (res == -1) {
		perror("setsockopt");
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(param.port);

	ssize_t res2 = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
	if (res2 == -1) {
		perror("bind");
		return -1;
	}

	res = listen(sd, 5);
	if (res == -1) {
		perror("listen");
		return -1;
	}

	while (true) {
		struct sockaddr_in cli_addr;
		socklen_t len = sizeof(cli_addr);
		int cli_sd = ::accept(sd, (struct sockaddr *)&cli_addr, &len);
		if (cli_sd == -1) {
			perror("accept");
			break;
		}
		std::thread* t = new std::thread(recvThread, cli_sd);
		t->detach();
	}
	::close(sd);
}
