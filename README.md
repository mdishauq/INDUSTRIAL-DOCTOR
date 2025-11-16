# AI-Bot-HIKMA

# 🎓 **FINAL YEAR PROJECT PPT CONTENT**

### **AI-Powered Voice Interactive Robot Car (Using Raspberry Pi 5)**

---

# **SLIDE 1 — Title Slide**

**Project Title:**
**AI Voice-Controlled Autonomous Robot Car**

**Team Members:**
Your Name(s)

**Guide:**
Guide Name

**Department of Computer Science / Electronics**
College Name

---

# **SLIDE 2 — Introduction**

* This project is an AI-enabled robot car capable of:

  * Listening to human speech
  * Understanding commands
  * Responding with natural voice
  * Moving based on voice instructions
  * Avoiding obstacles autonomously
* The system runs **offline**, with no internet required.

---

# **SLIDE 3 — Project Goals**

1. Build a robot that can interact through natural speech
2. Perform real-time voice commands for motion
3. Provide conversational responses using AI
4. Implement safe autonomous navigation
5. Completely offline functioning (fast + private)

---

# **SLIDE 4 — System Overview**

The system consists of:

* Raspberry Pi 5 running the AI engine
* USB microphone for voice input
* Vosk for Speech-to-Text
* Ollama (Phi-3.5) for AI reasoning
* Coqui TTS for voice output
* DC motors + L298N for motion
* Ultrasonic sensor for obstacle avoidance
* Separate power supplies for Pi and motors

---

# **SLIDE 5 — Hardware Components**

1. **Raspberry Pi 5 – 8GB**
2. **BassDrops Mini USB Microphone**
3. **Mini USB Desktop Speaker**
4. **L298N Motor Driver**
5. **DC Geared Motors + Wheels + Caster Wheel**
6. **HC-SR04 Ultrasonic Sensor**
7. **10,000mAh Power Bank (for Pi)**
8. **2 × 18650 Li-ion Batteries + Holder (for motors)**
9. Jumper wires, breadboard, frame, switches

---

# **SLIDE 6 — Hardware Architecture Diagram**

(Describe in text for now — you can convert into a block diagram)

```
Raspberry Pi 5
   ├── USB Microphone → Vosk STT
   ├── USB Speaker ← Coqui TTS
   ├── GPIO → L298N Motor Driver → Motors
   └── GPIO ← Ultrasonic Sensor (HC-SR04)
```

Separate Power Supplies:

* Power Bank → Raspberry Pi
* Li-ion Battery Pack → Motor Driver

---

# **SLIDE 7 — Software Components**

1. **Vosk (Offline Speech-to-Text)**
   Converts speech to text instantly

2. **Command Interpreter**
   Detects motion commands

3. **Ollama + Phi-3.5 (AI Brain)**
   Generates natural responses for non-command queries

4. **Coqui TTS (Text-to-Speech)**
   Converts AI text to voice

5. **Motor Control System**
   Controls direction and speed

6. **Obstacle Avoidance System**
   Uses HC-SR04 to avoid collisions

---

# **SLIDE 8 — Core Functionality Pipeline**

```
User Speech
   ↓
Microphone
   ↓
Vosk STT → Text
   ↓
Command Checker
      ├── If movement command → Motor Controller
      └── Else → Ollama (Phi-3.5)
                      ↓
                 AI Response
                      ↓
                Coqui TTS → Speaker
```

---

# **SLIDE 9 — Movement Command Flow**

**Commands:**

* Move forward
* Move backward
* Turn left
* Turn right
* Stop
* Autonomous mode

**Execution:**

1. STT detects command
2. Interpreter validates
3. Motor driver receives GPIO signals
4. Robot moves accordingly

---

# **SLIDE 10 — AI Response Flow**

1. User asks a general question
2. Vosk → text
3. Text sent to Ollama (phi-3.5)
4. AI generates answer
5. Coqui TTS converts text → voice
6. Speaker outputs reply

---

# **SLIDE 11 — Obstacle Avoidance System**

* Ultrasonic sensor continuously checks distance
* If object < 20 cm:

  * Stop
  * Turn left/right
  * Move forward again
* Ensures safety + autonomous driving mode

---

# **SLIDE 12 — Why Offline AI?**

* Very fast (low latency)
* No internet required
* Works in any environment
* Private and secure
* Perfect for embedded robotics

---

# **SLIDE 13 — Expected Results**

* Robot moves according to voice commands
* Smooth autonomous navigation
* Natural conversational replies
* Fast response due to local AI model
* Hands-free intelligent robot assistant

---

# **SLIDE 14 — Applications**

* Smart assistant robots
* Home automation
* Robotics education
* Voice-controlled delivery bots
* Companion robots
* Autonomous navigation research

---

# **SLIDE 15 — Limitations**

* Pi 5 can heat under heavy AI load (requires cooling)
* Microphone accuracy reduces in noisy rooms
* Coqui TTS takes some CPU power
* Motor battery drains faster during long use

---

# **SLIDE 16 — Future Enhancements**

* Add camera + computer vision
* Implement face following
* Add GPS navigation
* Integrate deep-learning motion planning
* Add mobile app control
* Add multiple sensors for full autonomy

---

# **SLIDE 17 — Conclusion**

* An offline, fully functional AI robot
* Understands speech
* Moves based on commands
* Responds with intelligent conversation
* Avoids obstacles autonomously
* Combines AI + Robotics + Embedded Systems

---

# **SLIDE 18 — Thank You**

Questions?
