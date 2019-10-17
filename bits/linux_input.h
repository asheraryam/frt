// linux_input.h
/*
 * FRT - A Godot platform targeting single board computers
 * Copyright (c) 2017-2019  Emanuele Fornara
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>

namespace frt {

static const char *dev_input_id_dir = "/dev/input/by-id";

enum KeyValue { KV_Released,
				KV_Pressed,
				KV_Repeated };

class LinuxInput {
private:
	int fd;
	bool grabbed;
	bool find_by_name(char *buf, int size, const char *name) {
		const char *s_name = "N: Name=\"";
		const char *s_handlers = "H: Handlers=";
		const char *s_event = "event";
		FILE *f = fopen("/proc/bus/input/devices", "r");
		if (!f)
			return false;
		char s[1024];
		bool found = false;
		int state = 0, nn = strlen(name), ns;
		while (fgets(s, sizeof(s), f) && !found) {
			switch (state) {
			case 0:
				if (!strncmp(s_name, s, strlen(s_name))) {
					ns = strlen(s);
					if (((int)(ns - strlen(s_name) - strlen("\"\n")) == nn)
					  && !memcmp(&s[strlen(s_name)], name, nn))
						state = 1;
				}
				break;
			case 1:
				if (!strncmp(s_handlers, s, strlen(s_handlers))) {
					state = 0;
					char *event = strstr(s, s_event);
					if (!event)
						break;
					int id = atoi(event + strlen(s_event));
					snprintf(buf, size, "/dev/input/event%d", id);
					found = true;
				}
				break;
			}
		}
		fclose(f);
		return found;
	}
	bool open_file(const char *file_name) {
		fd = ::open(file_name, O_RDONLY | O_NONBLOCK);
		if (fd == -1)
			return false;
		return true;
	}
	bool parse_dir(const char *pattern, char *buf, int size) {
		bool found = false;
		DIR *dirp = opendir(dev_input_id_dir);
		if (!dirp)
			return false;
		while (1) {
			struct dirent *dp;
			if (!(dp = readdir(dirp)))
				break;
			const char *id = dp->d_name;
			if (strstr(id, pattern)) {
				snprintf(buf, size, "%s/%s", dev_input_id_dir, id);
				buf[size - 1] = '\0';
				found = true;
				break;
			}
		}
		closedir(dirp);
		return found;
	}
	bool open_by_id_substr(const char *substr) {
		char buf[512];
		if (!parse_dir(substr, buf, sizeof(buf)))
			return false;
		return open_file(buf);
	}

protected:
	LinuxInput()
		: fd(-1), grabbed(false) {}
	bool open(const char *env_tag, const char *substr) {
		char *id = getenv(env_tag);
		if (id) {
			char buf[512];
			const char *s = 0;
			if (id[0] == '/')
				s = id;
			else if (find_by_name(buf, sizeof(buf), id))
				s = buf;
			if (s)
				return open_file(s);
		} else {
			return open_by_id_substr(substr);
		}
		return false;
	}
	bool grab(bool grab, int wait_ms) {
		/*
			HACK to let other clients get release keys at startup
			TODO: better handling
		 */
		if (this->grabbed == grab)
			return true;
		usleep(wait_ms * 1000);
		if (ioctl(fd, EVIOCGRAB, grab ? 1 : 0))
			return false;
		this->grabbed = grab;
		return true;
	}
	void close() {
		if (fd != -1) {
			::close(fd);
			fd = -1;
		}
	}
	bool poll() {
		input_event events[64];
		while (1) {
			int bytes = read(fd, events, sizeof(events));
			if (bytes <= 0)
				return false;
			int n = bytes / sizeof(input_event);
			for (int i = 0; i < n; i++)
				handle(events[i]);
		}
	}
	// LinuxInput
	virtual void handle(const input_event &ev) = 0;
};

} // namespace frt
