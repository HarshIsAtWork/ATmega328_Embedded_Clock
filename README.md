# ATmega328_Embedded_Clock

# H328

### A custom ATmega328 development board, built into a desk clock.

H328 is a custom development board designed around the **ATmega328**, with its first job being a simple desk clock.

Instead of using an Arduino UNO, I designed the board myself — including the PCB, connections, and a **2×3 ICSP programming header** that is directly compatible with the Arduino UNO programming interface.

The result is a small, reusable ATmega328 platform that can be used for much more than just a clock.

---

## What makes H328 interesting?

The main idea behind H328 was simple:

> **Why use an Arduino board when you can build your own?**

The board keeps the familiar ATmega328 architecture while giving me complete control over the hardware.

### Key features

* **ATmega328-based** custom development board
* Custom PCB designed from scratch
* **2×3 ICSP programming header**
* Directly compatible with the **Arduino UNO ICSP interface**
* Used as the platform for a functional desk clock
* Designed to be reusable for future ATmega328 projects

---

## The Desk Clock

The first application for H328 is a standalone desk clock.

Building the clock gave me a practical way to test the board rather than simply checking whether the ATmega328 could run a basic program.

It also makes H328 an actual usable device instead of another development board that ends up in a drawer.

---

## Why I built it

I've used plenty of Arduino boards, but H328 was about understanding what goes into making one.

Designing the board meant working out how the ATmega328 should be connected, designing the PCB, adding the programming interface, and then putting the finished board to work.

The desk clock is just the beginning.

---

## Hardware

| Part                | Purpose                   |
| ------------------- | ------------------------- |
| **ATmega328**       | Main microcontroller      |
| **2×3 ICSP Header** | Programming the ATmega328 |
| **Clock display**   | Displays the current time |
| **Custom PCB**      | Holds everything together |

---

## Future Plans

H328 is designed to be reusable, so the clock doesn't have to be its final form.

Some possible future upgrades:

* Additional sensors
* More display options
* Better power management
* Custom enclosure
* More I/O brought out to headers
* Other ATmega328-based projects

---

## Project Status

**Status:** Functional
**Platform:** Custom ATmega328 Development Board
**Programming:** 2×3 ICSP
**Compatibility:** Arduino UNO ICSP

---

### Built from scratch.

**H328** started as a development board and became a desk clock.
Now it has an excuse to become several other things.

---
