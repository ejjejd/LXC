#include <iostream>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <string.h>
#include <fcntl.h>

#define STACK_SIZE	65536
#define LOAD_PROCESS	"/bin/sh" //Load the shell process
#define MAX_PIDS	"10"

#define CGROUP_FOLDER 	"/sys/fs/cgroup/pids/container/"

#define Concat(a, b) (a"" b)

char* StackMemory()
{
	auto* stack = new(std::nothrow) char [STACK_SIZE];

	if(stack == nullptr)
	{
		printf("Cannot allocate memory!\n");
		exit(EXIT_FAILURE);
	}

	return stack + STACK_SIZE;
}

void WriteRule(const char* path, const char* value)
{
	int fp = open(path, O_WRONLY | O_APPEND);
	write(fp, value, strlen(value));
	close(fp);
}

void LimitProcessCreation()
{
	mkdir(CGROUP_FOLDER, S_IRUSR | S_IWUSR);

	const char* pid = std::to_string(getpid()).c_str();

	WriteRule(Concat(CGROUP_FOLDER, "cgroup.procs"), pid);
	WriteRule(Concat(CGROUP_FOLDER, "notify_on_release"), "1");
	WriteRule(Concat(CGROUP_FOLDER, "pids.max"), MAX_PIDS);
}

template<typename... P>
int Run(P... params)
{
	char* args[] = {(char*)params..., (char*)0};
	return execvp(args[0], args);
}

void SetupRoot(const char* folder)
{
	chroot(folder);
	chdir("/");
}

void SetupVariables()
{
	clearenv();
	setenv("TERM", "xterm-256color", 0);
	setenv("PATH", "/bin/:/sbin/:usr/bin:/usr/sbin", 0);
}

template<typename F>
void CloneProcess(F&& function, int flags)
{
	auto pid = clone(function, StackMemory(), flags, 0);
	
	wait(nullptr);
}

int Jail(void* args)
{
	LimitProcessCreation();

	printf("Child process: %d\n", getpid());

	SetupVariables();
	SetupRoot("./root");

	mount("proc", "/proc", "proc", 0, 0);

	auto runThis = [](void* args) -> int { Run(LOAD_PROCESS); };

	CloneProcess(runThis, SIGCHLD);

	umount("/proc");

	return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
	printf("Hello World! (parent)\n");
	printf("Parent pid: %d\n", getpid());

	CloneProcess(Jail, CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD); 

	//SIGCHLD      - process emit a signal when finished						      	 		      
	//CLONE_NEWUTS - get a copy of the global UTS						      
	//CLONE_NEWPID - gives cloned process it's own process tree

	
	wait(nullptr);	

	return EXIT_SUCCESS;
}
