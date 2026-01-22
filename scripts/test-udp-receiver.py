#!/usr/bin/env python3
"""
OpenTrack UDP Protocol Test Script

Sends simulated head tracking data to the UDP receiver for testing.
OpenTrack protocol: 6 little-endian doubles (48 bytes)
  [X, Y, Z, Yaw, Pitch, Roll]

Usage:
  python test-udp-receiver.py                 # Send single test packet
  python test-udp-receiver.py --continuous    # Send continuous data
  python test-udp-receiver.py --sweep         # Sweep through rotation range
"""

import socket
import struct
import argparse
import time
import math
import sys

DEFAULT_IP = "127.0.0.1"
DEFAULT_PORT = 4242

def create_packet(x: float, y: float, z: float,
                  yaw: float, pitch: float, roll: float) -> bytes:
    """Create an OpenTrack UDP packet.

    Args:
        x, y, z: Translation values (mm) - not used by this mod
        yaw, pitch, roll: Rotation values (degrees)

    Returns:
        48-byte packet in OpenTrack format
    """
    return struct.pack('<6d', x, y, z, yaw, pitch, roll)


def send_single_packet(ip: str, port: int,
                       yaw: float, pitch: float, roll: float) -> None:
    """Send a single test packet."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    packet = create_packet(0.0, 0.0, 0.0, yaw, pitch, roll)
    sock.sendto(packet, (ip, port))
    sock.close()
    print(f"Sent: yaw={yaw:.2f}, pitch={pitch:.2f}, roll={roll:.2f}")


def send_continuous(ip: str, port: int, rate_hz: float) -> None:
    """Send continuous oscillating data."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    interval = 1.0 / rate_hz
    start_time = time.time()
    packet_count = 0

    print(f"Sending continuous data to {ip}:{port} at {rate_hz} Hz")
    print("Press Ctrl+C to stop")

    try:
        while True:
            elapsed = time.time() - start_time

            # Oscillate yaw: -45 to +45 degrees over 4 seconds
            yaw = 45.0 * math.sin(elapsed * math.pi / 2.0)
            # Oscillate pitch: -30 to +30 degrees over 3 seconds
            pitch = 30.0 * math.sin(elapsed * 2.0 * math.pi / 3.0)
            # Oscillate roll: -15 to +15 degrees over 2 seconds
            roll = 15.0 * math.sin(elapsed * math.pi)

            packet = create_packet(0.0, 0.0, 0.0, yaw, pitch, roll)
            sock.sendto(packet, (ip, port))
            packet_count += 1

            if packet_count % int(rate_hz) == 0:
                print(f"[{elapsed:.1f}s] yaw={yaw:+.1f}, pitch={pitch:+.1f}, roll={roll:+.1f}")

            time.sleep(interval)

    except KeyboardInterrupt:
        elapsed = time.time() - start_time
        print(f"\nStopped after {elapsed:.1f}s, sent {packet_count} packets")
    finally:
        sock.close()


def send_sweep(ip: str, port: int, duration: float) -> None:
    """Sweep through rotation range for calibration testing."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    rate_hz = 60.0
    interval = 1.0 / rate_hz
    total_steps = int(duration * rate_hz)

    print(f"Sweeping rotation range to {ip}:{port}")
    print(f"Duration: {duration}s, Rate: {rate_hz} Hz")

    try:
        # Phase 1: Yaw sweep
        print("\nPhase 1: Yaw sweep (-90 to +90)")
        for i in range(total_steps // 3):
            progress = i / (total_steps // 3)
            yaw = -90.0 + 180.0 * progress
            packet = create_packet(0, 0, 0, yaw, 0, 0)
            sock.sendto(packet, (ip, port))
            time.sleep(interval)
        print("  Done")

        # Phase 2: Pitch sweep
        print("Phase 2: Pitch sweep (-60 to +60)")
        for i in range(total_steps // 3):
            progress = i / (total_steps // 3)
            pitch = -60.0 + 120.0 * progress
            packet = create_packet(0, 0, 0, 0, pitch, 0)
            sock.sendto(packet, (ip, port))
            time.sleep(interval)
        print("  Done")

        # Phase 3: Roll sweep
        print("Phase 3: Roll sweep (-45 to +45)")
        for i in range(total_steps // 3):
            progress = i / (total_steps // 3)
            roll = -45.0 + 90.0 * progress
            packet = create_packet(0, 0, 0, 0, 0, roll)
            sock.sendto(packet, (ip, port))
            time.sleep(interval)
        print("  Done")

        print("\nSweep complete")

    except KeyboardInterrupt:
        print("\nSweep interrupted")
    finally:
        sock.close()


def test_packet_structure() -> None:
    """Verify packet structure is correct."""
    packet = create_packet(1.0, 2.0, 3.0, 45.0, 30.0, 15.0)
    assert len(packet) == 48, f"Packet size should be 48, got {len(packet)}"

    # Unpack and verify
    values = struct.unpack('<6d', packet)
    assert values == (1.0, 2.0, 3.0, 45.0, 30.0, 15.0), f"Unexpected values: {values}"

    print("Packet structure test: PASSED")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="OpenTrack UDP protocol test script for DL2 Head Tracking mod"
    )
    parser.add_argument('--ip', default=DEFAULT_IP, help=f'Target IP (default: {DEFAULT_IP})')
    parser.add_argument('--port', type=int, default=DEFAULT_PORT, help=f'Target port (default: {DEFAULT_PORT})')
    parser.add_argument('--yaw', type=float, default=0.0, help='Yaw angle in degrees')
    parser.add_argument('--pitch', type=float, default=0.0, help='Pitch angle in degrees')
    parser.add_argument('--roll', type=float, default=0.0, help='Roll angle in degrees')
    parser.add_argument('--continuous', action='store_true', help='Send continuous oscillating data')
    parser.add_argument('--sweep', action='store_true', help='Sweep through rotation range')
    parser.add_argument('--rate', type=float, default=120.0, help='Update rate in Hz (default: 120)')
    parser.add_argument('--duration', type=float, default=9.0, help='Sweep duration in seconds (default: 9)')
    parser.add_argument('--test', action='store_true', help='Run packet structure test')

    args = parser.parse_args()

    if args.test:
        test_packet_structure()
        return 0

    print(f"OpenTrack UDP Test - Target: {args.ip}:{args.port}")
    print("=" * 50)

    if args.continuous:
        send_continuous(args.ip, args.port, args.rate)
    elif args.sweep:
        send_sweep(args.ip, args.port, args.duration)
    else:
        send_single_packet(args.ip, args.port, args.yaw, args.pitch, args.roll)

    return 0


if __name__ == '__main__':
    sys.exit(main())
