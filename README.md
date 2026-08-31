# ATmega328_Embedded_Clock

# H328

### A custom ATmega328 development board turned into a fully featured desk clock.

H328 is a **custom ATmega328-based development board** that I designed and built into a standalone desk clock.

It combines a custom PCB, **DS3231 real-time clock**, IR remote control, custom menus, calendar, Pomodoro timer, WS2812B lighting, and a rechargeable **18650 battery with a 5V boost supply** into one compact device.

The goal was simple: build a desk clock that actually feels like a proper gadget, while designing the hardware behind it myself.

---

## What can it do?

H328 is much more than a basic clock.

### Clock & Calendar

* Accurate timekeeping with the **DS3231 RTC**
* <img width="1600" height="720" alt="home" src="https://github.com/user-attachments/assets/dc73c7ab-3d13-4ecf-92d5-2826d183c8b2" />

* Date and calendar display
* Custom on-screen menus
* <img width="1600" height="720" alt="alarm" src="https://github.com/user-attachments/assets/d15fdd1a-4101-4514-a425-6c982bc9e530" />

* Easy navigation through the different functions

#### clock 
<img width="1600" height="720" alt="home" src="https://github.com/user-attachments/assets/98bc313c-1ad4-4b4c-acbd-8b21fd1c7ee3" />
#### calendar
<img width="1600" height="720" alt="calendarrr" src="https://github.com/user-attachments/assets/c3af3928-e145-470d-9ec1-76b667bb0a38" />
<img width="1600" height="720" alt="calendar" src="https://github.com/user-attachments/assets/2e54e041-4f5e-4044-8b3b-d46cccab3922" />


### Productivity

* **Pomodoro timer** for focused work sessions
* <img width="1600" height="720" alt="pomodoro" src="https://github.com/user-attachments/assets/9ef05575-57fb-45ec-b5e2-3effdbf22da4" />
<img width="1600" height="720" alt="pomo_menu" src="https://github.com/user-attachments/assets/53f7eb0e-2347-43a0-9f80-d0d385fe337a" />
<img width="1600" height="720" alt="pomo" src="https://github.com/user-attachments/assets/ad89a92e-4426-4bce-b6ef-5c16fad6d81a" />


* Dedicated timer functionality
* Designed to stay on the desk and be useful throughout the day

### Lighting

* Built-in **WS2812B RGB lighting**
* <img width="1600" height="720" alt="ledcolor" src="https://github.com/user-attachments/assets/0b1100d6-5be5-4abf-989e-3ec5d82f69fb" />

* Custom lighting effects
* <img width="1600" height="720" alt="ledcolormenu" src="https://github.com/user-attachments/assets/aabd4b6c-df42-493c-9919-e2bcf9865abe" />

* Can be used as ambient desk lighting or as part of the clock's interface

### Remote Control

* Built-in **IR receiver**
* Control the clock without having to reach for it
* Opens the door for additional remote-controlled features

### Portable Power

H328 isn't tied to a wall outlet.

* Rechargeable **18650 lithium-ion cell**
* **5V boost converter** for powering the system
* Designed to work as a standalone desk device

### 30 SEC SLEEP
* the device sleeps after 30 sec of inactivity to dramatically improve battery life
* battery can last upto one week. (tested)
* although it can vary upon usage.

---

## The Hardware

At the heart of H328 is the **ATmega328**.

But rather than putting it on an Arduino UNO, I designed my own development board around it.

The board includes a custom **2×3 ICSP programming header**, making it directly compatible with the programming interface used by the Arduino UNO.

This means the board can be programmed and reused for other ATmega328 projects instead of being locked into being just a clock.

### Main hardware

| Component           | Purpose                            |
| ------------------- | ---------------------------------- |
| **ATmega328**       | Main controller                    |
| **DS3231 RTC**      | Accurate time and date             |
| **IR Receiver**     | Wireless control                   |
| **WS2812B LEDs**    | RGB lighting and effects           |
| **18650 Cell**      | Rechargeable power source          |
| **5V Boost Module** | Provides the required 5V supply    |
| **2×3 ICSP Header** | Arduino UNO-compatible programming |

---

## The Software

The software was built specifically for H328 rather than relying on a generic clock interface.

It has a custom menu system that brings the different functions together in one interface.

The current software includes:

* Clock
* Calendar
* Timers
* Pomodoro
* Custom menus
* IR remote control
* WS2812B lighting control

The idea is to make the device feel like a small dedicated product rather than a microcontroller demo.

---

## Why I built it

I've built plenty of projects using development boards.

H328 was an attempt to flip that around.

Instead of starting with an Arduino UNO and building something on top of it, I started with the **ATmega328 itself** and designed the board I actually wanted.

Then I used that board to build something useful.

The desk clock became a way to test the hardware, software, power system, user interface, and all the little details that come with making a complete standalone device.

---

## What's next?

H328 was designed to be reusable, so the clock is only its first application.

Future versions could bring:

* A cleaner and smaller PCB
* Better power management
* More sensors
* Additional display features
* More lighting effects
* A custom enclosure
* More ATmega328 projects built around the same board

---

## Project Status

**Status:** Functional
**Controller:** ATmega328
**RTC:** DS3231
**Lighting:** WS2812B
**Power:** Rechargeable 18650 + 5V boost
**Programming:** 2×3 ICSP
**Control:** IR Remote

---

## Built, not bought.

H328 started as a custom ATmega328 development board.

It ended up becoming a clock, calendar, Pomodoro timer, remote-controlled gadget, RGB desk light, and a pretty good excuse to design another PCB.

**And that's probably not the last thing it's going to become.**

---
