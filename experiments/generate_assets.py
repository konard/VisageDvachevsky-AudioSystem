#!/usr/bin/env python3
"""Generate placeholder assets for sample_novel example project."""

from PIL import Image, ImageDraw, ImageFont
import os

def create_placeholder_image(width, height, bg_color, text, filename):
    """Create a simple placeholder image with text."""
    img = Image.new('RGB', (width, height), bg_color)
    draw = ImageDraw.Draw(img)

    # Try to use a default font, fall back to basic if not available
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 48)
    except:
        font = ImageFont.load_default()

    # Calculate text position (center)
    bbox = draw.textbbox((0, 0), text, font=font)
    text_width = bbox[2] - bbox[0]
    text_height = bbox[3] - bbox[1]
    x = (width - text_width) // 2
    y = (height - text_height) // 2

    # Draw text with shadow for readability
    draw.text((x+2, y+2), text, fill=(0, 0, 0), font=font)
    draw.text((x, y), text, fill=(255, 255, 255), font=font)

    # Save the image
    img.save(filename, 'PNG')
    print(f"Created: {filename}")

def main():
    base_path = "examples/sample_novel/assets/images"
    os.makedirs(base_path, exist_ok=True)

    # Create background images
    create_placeholder_image(1920, 1080, (64, 96, 128), "Laboratory Room",
                            f"{base_path}/bg_room.png")
    create_placeholder_image(1920, 1080, (32, 48, 64), "Corridor",
                            f"{base_path}/bg_corridor.png")
    create_placeholder_image(1920, 1080, (96, 128, 160), "Observation Deck",
                            f"{base_path}/bg_deck.png")

    # Create character sprites (simpler, portrait-oriented)
    create_placeholder_image(512, 1024, (180, 140, 100), "Alice",
                            f"{base_path}/char_alice.png")
    create_placeholder_image(512, 1024, (100, 120, 140), "Bob",
                            f"{base_path}/char_bob.png")
    create_placeholder_image(512, 1024, (140, 100, 120), "System AI",
                            f"{base_path}/char_ai.png")

if __name__ == "__main__":
    main()
