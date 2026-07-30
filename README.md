# Followup Product Introduction

Followup is a place to capture your thoughts — whether it's an idea, a to-do, or just a note. Record what's on your mind at that light-bulb moment, before it slips away, and Followup helps you organize it afterward. With Gemini, your recordings are transcribed and summarized automatically. Everything is stored on your SD card.

It runs on the [Waveshare ESP32-S3-ePaper-3.97](https://docs.waveshare.com/ESP32-S3-ePaper-3.97), so your thoughts live on a quiet, always-on screen you can place anywhere — a constant, low-interruption reminder instead of one more notification buried in your phone.

## One-Sentence Positioning

**Followup is a voice-first thought-capture companion on always-on ePaper: record ideas, to-dos, and notes in the moment, let Gemini transcribe and summarize them, and keep the ones that matter in front of you as stickies.**

## What It Is Suitable For

- Capturing a sudden idea by voice at the light-bulb moment, before it's forgotten
- Jotting quick to-dos and notes hands-free while you're in the middle of something else
- Keeping a small, always-visible set of follow-ups on a desk, fridge, or wall
- Revisiting past ideas later to decide what's still worth pursuing
- Anyone who wants their thoughts organized without living inside another app on their phone

## Key Features

### 1. Capture at the Light-Bulb Moment

Press record and speak. Every capture starts as a voice recording, tagged as an **Idea**, a **To-do**, or a **Note**, so you can get the thought down the instant it arrives without stopping to type.

### 2. Gemini Transcription and Summarization

Once a recording is saved, Gemini transcribes the audio and summarizes it — turning a rambling voice memo into readable text and a concise summary you can scan at a glance.

A Gemini API key from [Google AI Studio](https://aistudio.google.com/) is required. You can get started on the free tier, subject to Gemini's free-tier limits, or use a paid account to transcribe without those limits.

### 3. Everything Stored on Your SD Card

Recordings, transcripts, and summaries are stored locally on the device's SD card. Your thoughts stay with you, on your own storage.

### 4. Vibe-Check Your Ideas

Ideas don't all age well. Review each one and decide whether it's still a vibe worth keeping — or something to trash so you can move on with a clear head.

### 5. Follow Up on Tasks and Notes

Mark a task or note as a follow-up to keep it on your radar. Followup helps you stay on track and focused on what actually needs doing next.

### 6. Display Your Follow-Ups as Stickies

Pin your follow-ups to the ePaper display as sticky notes. Because the screen is always on and low-power, they stay in front of you as a constant, gentle reminder.

## Typical Applications

| Application | Description |
| --- | --- |
| Idea | Capture a spark by voice and revisit it later with a vibe check |
| To-do | Record a task hands-free and follow up until it's done |
| Note | Keep a quick thought or reminder, transcribed and summarized |
| Follow-up | Flag the items that matter so they stay top of mind |
| Stickies | Display your active follow-ups on the ePaper as always-on reminders |
| Summaries | Let Gemini condense long recordings into a glanceable summary |

## Brief Specifications

Followup runs on the [Waveshare ESP32-S3-ePaper-3.97](https://docs.waveshare.com/ESP32-S3-ePaper-3.97).

| Item | Information |
| --- | --- |
| Product Name | Followup (on ESP32-S3-ePaper-3.97) |
| Product Type | Voice-capture notes app on an ePaper terminal |
| MCU | ESP32-S3R8, dual-core Xtensa LX7 up to 240MHz |
| Memory | 8MB PSRAM, 16MB flash |
| Screen | 3.97-inch black-and-white ePaper, 800 x 480, SSD1677 controller |
| Interaction | Buttons only — this board has no touchscreen (see [Controls](#controls)) |
| Connectivity | 2.4GHz Wi-Fi (802.11 b/g/n), Bluetooth 5 (LE) |
| Audio | ES8311 codec, onboard microphone, NS4150B amplifier, speaker header |
| Sensors | QMI8658 6-axis IMU, PCF85063 real-time clock |
| Power | AXP2101 PMIC, 3.7V lithium battery (MX1.25 connector), USB-C charging |
| Storage | microSD card (recordings, transcripts, summaries) |
| AI | Gemini (cloud) transcription and summarization, over Wi-Fi |

The board also carries an SHTC3 temperature/humidity sensor on the shared I2C bus. Followup does not currently read it.

## Controls

Followup is driven entirely by the three physical controls: a rocker, the BOOT button, and the PWR button.

| Control | Action |
| --- | --- |
| Rocker up / down | Move the selection; hold to repeat |
| Rocker down, held | Back out of a list or card you have entered |
| Rocker middle | Select / confirm |
| BOOT, tap | Select / confirm |
| BOOT, press and hold | Record — recording starts on the hold and stops when you let go |
| PWR, tap | Lock the screen, or unlock it |
| PWR, hold ~1s | Open the shutdown confirmation |
| PWR, hold 6s | Hardware power-off, straight from the PMIC |

Recording is exclusive to BOOT, so no other control can start or stop a capture by accident. The 6-second PWR hold bypasses the firmware entirely and always cuts power.

## Product Value Summary

The value of Followup is a quiet, always-visible place to catch your thoughts and keep the important ones in front of you. Instead of losing an idea to a forgotten note app or burying a task in a notification stream, you speak it in the moment, let Gemini turn it into clean text and a summary, and keep everything private on your SD card.

Ideas get a vibe check so you only carry forward what still matters. Tasks and notes become follow-ups so you stay on track. And the ones you care about most sit on the ePaper as stickies — a steady, low-interruption reminder of what's next.
