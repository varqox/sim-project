#pragma once

#include <string>
#include <deque>
#include <sys/shm.h>
#include <sys/stat.h>

void remove_r(const char* path);
std::string myto_string(long long int a);
std::string make_safe_php_string(const std::string& str);
std::string make_safe_html_string(const std::string& str);
std::deque<unsigned> kmp(const std::string& text, const std::string& pattern);
std::string file_get_contents(const std::string& file_name);

class SharedMemorySegment {
private:
	int id_;
	void* addr_;
	SharedMemorySegment(const SharedMemorySegment&);
	SharedMemorySegment& operator=(const SharedMemorySegment&);

public:
	SharedMemorySegment(size_t size): id_(shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)), addr_(NULL) {
		if (id_ != -1) {
			if ((addr_ = shmat(id_, NULL, 0)) == (void*)-1)
				addr_ = NULL;
			shmctl(id_, IPC_RMID, NULL);
		}
	}

	~SharedMemorySegment() {
		if (addr_)
			shmdt(addr_);
	}

	int key() const { return id_; }

	void* addr() const { return addr_; }
};
