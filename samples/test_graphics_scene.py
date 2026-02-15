import robocute as rbc
import argparse


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-b",
        "--backend",
        type=str,
        default="dx",
        help="graphics backend api type, dx/vk",
    )
    parser.add_argument(
        "-s",
        "--scene",
        type=str,
        help="rbc scene path, the directory containing rbc_project.json",
        required=True,
    )
    args = parser.parse_args()

    print(args.scene)


if __name__ == "__main__":
    main()
