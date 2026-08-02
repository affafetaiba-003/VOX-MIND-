# VOX-MIND-
A C++ / Qt Desktop Voice Assistant with Native Windows Speech Recognition

VoxMind is a desktop voice assistant built with C++ and the Qt framework, designed to let users interact with their system quickly through natural speech instead of typing or clicking.It also holds the feature of accessing through typing.It combines a responsive Qt-based interface with Microsoft's native Speech API (SAPI) to deliver real-time voice recognition and spoken responses, executing system-level commands directly from voice input.
**✨ Features
🌟 Highlights**
[Standout Feature 1] — e.g. real-time wake-word detection, continuous listening mode
[Standout Feature 2] — e.g. natural language command parsing beyond fixed keywords
[Standout Feature 3] — e.g. multi-threaded architecture keeping the UI responsive during recognition
**Core Functionality**
Voice Command Recognition — Converts spoken input into executable system commands
Text-to-Speech Feedback — Responds audibly to confirm actions or answer queries
[Application Control] — e.g. open/close applications by voice
[System Commands] — e.g. shutdown, restart, volume control, file operations
[Query Handling] — e.g. time, date, web search
Activity Logging — Records session history to VoxMind.log (see below)
**🛠️ Tech Stack**
Core Language	C++
GUI Framework	Qt
Speech Recognition & Synthesis	Microsoft SAPI (Speech API)
Command Execution	Windows built-in commands / system calls
Platform	Windows
**On SAPI specifically:**
VoxMind uses Microsoft's Speech API (ISpVoice / ISpRecoContext) for both directions of speech interaction — converting recognized speech into text commands, and synthesizing spoken responses back to the user, without relying on any external cloud-based speech service.
**📜 VoxMind.log**

VoxMind maintains a log file, VoxMind.log, which records the assistant's session history
**👥 Team****(AUR)**
Affaf
Rameeha
Ubaid
