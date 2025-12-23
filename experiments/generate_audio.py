#!/usr/bin/env python3
"""Generate placeholder audio files for sample_novel example project."""

import numpy as np
import wave
import struct
import os

def generate_sine_wave(frequency, duration, sample_rate=44100, amplitude=0.3):
    """Generate a sine wave."""
    t = np.linspace(0, duration, int(sample_rate * duration))
    wave_data = amplitude * np.sin(2 * np.pi * frequency * t)
    return wave_data

def save_wav(filename, data, sample_rate=44100):
    """Save audio data as WAV file."""
    # Convert to 16-bit PCM
    scaled_data = np.int16(data * 32767)

    with wave.open(filename, 'w') as wav_file:
        wav_file.setnchannels(1)  # Mono
        wav_file.setsampwidth(2)  # 16-bit
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(scaled_data.tobytes())

    print(f"Created: {filename}")

def generate_music_loop(filename, duration=10):
    """Generate a simple music loop with multiple harmonics."""
    sample_rate = 44100

    # Create a simple chord progression
    base_freq = 220  # A3
    chord1 = (generate_sine_wave(base_freq, duration/2, sample_rate, 0.2) +
              generate_sine_wave(base_freq * 1.25, duration/2, sample_rate, 0.15) +
              generate_sine_wave(base_freq * 1.5, duration/2, sample_rate, 0.1))

    chord2 = (generate_sine_wave(base_freq * 0.75, duration/2, sample_rate, 0.2) +
              generate_sine_wave(base_freq * 0.9375, duration/2, sample_rate, 0.15) +
              generate_sine_wave(base_freq * 1.125, duration/2, sample_rate, 0.1))

    music = np.concatenate([chord1, chord2])

    # Apply fade in/out
    fade_samples = int(sample_rate * 0.5)
    fade_in = np.linspace(0, 1, fade_samples)
    fade_out = np.linspace(1, 0, fade_samples)
    music[:fade_samples] *= fade_in
    music[-fade_samples:] *= fade_out

    save_wav(filename, music, sample_rate)

def generate_click_sound(filename):
    """Generate a simple click/UI sound effect."""
    sample_rate = 44100
    duration = 0.1

    # Short burst at high frequency
    click = generate_sine_wave(1000, duration, sample_rate, 0.5)

    # Apply envelope (quick attack, quick decay)
    envelope = np.exp(-np.linspace(0, 10, len(click)))
    click *= envelope

    save_wav(filename, click, sample_rate)

def generate_footstep_sound(filename):
    """Generate a footstep sound effect."""
    sample_rate = 44100
    duration = 0.3

    # Low frequency thump
    footstep = generate_sine_wave(80, duration, sample_rate, 0.6)

    # Add some noise for texture
    noise = np.random.normal(0, 0.1, len(footstep))
    footstep = footstep + noise * 0.3

    # Apply envelope
    envelope = np.exp(-np.linspace(0, 8, len(footstep)))
    footstep *= envelope

    save_wav(filename, footstep, sample_rate)

def generate_voice_placeholder(filename, pitch=200, duration=2.0):
    """Generate a voice placeholder (simple tone with variation)."""
    sample_rate = 44100

    # Variable pitch to simulate speech
    t = np.linspace(0, duration, int(sample_rate * duration))
    pitch_variation = pitch + 20 * np.sin(2 * np.pi * 2 * t)

    voice = np.zeros_like(t)
    for i, p in enumerate(pitch_variation):
        voice[i] = 0.3 * np.sin(2 * np.pi * p * t[i])

    # Apply speech-like envelope
    envelope = np.ones_like(voice)
    # Add some amplitude variation
    envelope *= 0.7 + 0.3 * np.sin(2 * np.pi * 3 * t)

    # Fade in/out
    fade_samples = int(sample_rate * 0.1)
    fade_in = np.linspace(0, 1, fade_samples)
    fade_out = np.linspace(1, 0, fade_samples)
    envelope[:fade_samples] *= fade_in
    envelope[-fade_samples:] *= fade_out

    voice *= envelope

    save_wav(filename, voice, sample_rate)

def main():
    # Create directories
    os.makedirs("examples/sample_novel/assets/audio/music", exist_ok=True)
    os.makedirs("examples/sample_novel/assets/audio/sfx", exist_ok=True)
    os.makedirs("examples/sample_novel/assets/audio/voice", exist_ok=True)

    # Generate music
    print("Generating music...")
    generate_music_loop("examples/sample_novel/assets/audio/music/music_theme.wav", duration=8)

    # Generate SFX
    print("\nGenerating sound effects...")
    generate_click_sound("examples/sample_novel/assets/audio/sfx/sfx_click.wav")
    generate_footstep_sound("examples/sample_novel/assets/audio/sfx/sfx_door.wav")

    # Generate voice placeholders
    print("\nGenerating voice placeholders...")
    generate_voice_placeholder("examples/sample_novel/assets/audio/voice/alice_001.wav", pitch=250, duration=1.5)
    generate_voice_placeholder("examples/sample_novel/assets/audio/voice/alice_002.wav", pitch=260, duration=2.0)
    generate_voice_placeholder("examples/sample_novel/assets/audio/voice/bob_001.wav", pitch=180, duration=1.8)
    generate_voice_placeholder("examples/sample_novel/assets/audio/voice/bob_002.wav", pitch=175, duration=2.2)

    print("\n✓ All audio assets generated!")

if __name__ == "__main__":
    main()
