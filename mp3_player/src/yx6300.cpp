// src/mp3_node.cpp
#include <ros/ros.h>
#include <std_msgs/String.h>

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <algorithm>
#include <string>
#include <cctype>

static int serial_fd = -1;

// ===== UART 초기화 =====
bool init_serial(const char* port = "/dev/ttyAMA0", speed_t baud = B9600) {
  serial_fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
  if (serial_fd < 0) {
    ROS_ERROR("Failed to open serial port: %s", port);
    return false;
  }
  fcntl(serial_fd, F_SETFL, 0);

  termios tty{};
  if (tcgetattr(serial_fd, &tty) != 0) {
    ROS_ERROR("tcgetattr failed");
    return false;
  }

  cfmakeraw(&tty);
  cfsetispeed(&tty, baud);
  cfsetospeed(&tty, baud);

  tty.c_cflag |= (CLOCAL | CREAD);  // 수신 enable, 로컬
  tty.c_cflag &= ~PARENB;           // no parity
  tty.c_cflag &= ~CSTOPB;           // 1 stop bit
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;               // 8 data bits
  tty.c_cflag &= ~CRTSCTS;          // HW flow off

  tty.c_cc[VMIN]  = 0;
  tty.c_cc[VTIME] = 10;             // read timeout = 1.0s

  tcflush(serial_fd, TCIFLUSH);
  if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
    ROS_ERROR("tcsetattr failed");
    return false;
  }
  return true;
}

// ===== YX6300 패킷 전송 (DFPlayer 호환 포맷) =====
// 0x7E 0xFF 0x06 CMD 0x00 PARAM_H PARAM_L 0xEF
inline void send_packet(uint8_t cmd, uint16_t param = 0) {
  if (serial_fd < 0) return;
  uint8_t buf[8];
  buf[0] = 0x7E; buf[1] = 0xFF; buf[2] = 0x06; buf[3] = cmd; buf[4] = 0x00;
  buf[5] = (param >> 8) & 0xFF;
  buf[6] = (param     ) & 0xFF;
  buf[7] = 0xEF;
  write(serial_fd, buf, 8);
  usleep(100000); // 100ms 여유
}

inline void set_volume(int vol) {
  if (vol < 0) vol = 0;
  if (vol > 30) vol = 30;
  send_packet(0x06, (uint16_t)vol);
}

inline void play_track(uint16_t track_no) {
  if (track_no == 0) return;
  send_packet(0x03, track_no);  // 1 → 0001.mp3
}

// "0001.mp3" 또는 "1" → 1 로 변환 (테스트는 파일명 기준이면 이것만 사용)
static int parse_track(std::string s) {
  // 공백 제거
  s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());

  // 대문자화 후 ".MP3" 제거
  std::string up = s;
  std::transform(up.begin(), up.end(), up.begin(), ::toupper);
  if (up.size() >= 4 && up.substr(up.size()-4) == ".MP3")
    up = up.substr(0, up.size()-4);

  // 앞의 0 제거
  size_t nz = up.find_first_not_of('0');
  if (nz == std::string::npos) up = "0";
  else up = up.substr(nz);

  // 숫자면 정수 변환
  if (!up.empty() && std::all_of(up.begin(), up.end(), ::isdigit)) {
    return std::stoi(up);
  }
  return -1;
}

// ===== ROS 콜백: /mp3_cmd 에 문자열 오면 재생 =====
void mp3Callback(const std_msgs::String::ConstPtr& msg) {
  int track = parse_track(msg->data);
  if (track <= 0) {
    ROS_WARN("Bad audio command: %s", msg->data.c_str());
    return;
  }
  ROS_INFO("Play track %d", track);
  play_track((uint16_t)track);
}

int main(int argc, char** argv) {
  ros::init(argc, argv, "mp3_node");
  ros::NodeHandle nh;

  // UART 준비
  if (!init_serial("/dev/ttyAMA0", B9600)) return 1;
  ros::Duration(0.5).sleep(); // 안정화
  set_volume(25);

  // 토픽 구독 시작
  ros::Subscriber sub = nh.subscribe("/mp3_cmd", 10, mp3Callback);

  ROS_INFO("MP3 node ready. Waiting on /mp3_cmd (e.g., '0001.mp3')...");
  ros::spin();

  if (serial_fd >= 0) close(serial_fd);
  return 0;
}

