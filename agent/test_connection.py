from communicate import open_connection, read_game_state, send_move
import time

def main():
    conn = open_connection()

    print("Waiting for game state packets from FPGA...")
    print("Make sure your Nios V program is running.")
    print("Press Ctrl+C to stop.\n")

    packet_count = 0

    try:
        while True:
            state = read_game_state(conn)

            if state is None:
                continue

            packet_count += 1
            print(f"Packet {packet_count}:")
            print(f"  Player:  ({state['px']}, {state['py']})")
            print(f"  Target:  col={state['tcol']}, row={state['trow']}")
            print(f"  Map:     {state['map']}")

            # for now just send a random move to test the other direction
            import random
            move = random.randint(0, 3)
            send_move(conn, move)
            print(f"  Sent move: {move}")
            print()

            time.sleep(0.05)  # 20 times per second

    except KeyboardInterrupt:
        print(f"\nStopped. Received {packet_count} packets total.")
        conn.close()

if __name__ == '__main__':
    main()