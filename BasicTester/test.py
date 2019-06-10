#! python3
import sys
import os
import subprocess


class TestException(Exception):
    pass


class ProcessInterface:
    def __init__(self, bin_path):
        self._exe_path = bin_path
        self._proc = None

    def start(self, arguments=[]):
        self._proc = subprocess.Popen([self._exe_path] + arguments, stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0, )

    def send_command(self, command):
        print("Sent: " + command.decode('utf-8'))
        self._proc.stdin.write(command + b"\r\n")

    def get_stdout_line(self):
        return self._proc.stdout.readline().strip().decode("utf-8")

    def kill(self):
        if self._proc:
            self._proc.terminate()
            self._proc.wait()


def run_test(client_proc, server_proc):
    print("Starting server with one connection")
    server_proc.start(["1"])
    if server_proc.get_stdout_line() != "Success":
        raise TestException("Invalid output for server!")

    print("Starting client")
    client_proc.start()

    out_line = client_proc.get_stdout_line()
    if out_line != "Successful connection":
        raise TestException("Invalid output for client! Received: " + out_line)

    client_proc.send_command(b"echo simple test")
    out_line = client_proc.get_stdout_line()
    if out_line != "simple test":
        raise TestException(
            "Invalid output for client for echo! Received: " + out_line)

    out_line = server_proc.get_stdout_line()
    if out_line != "simple test":
        raise TestException(
            "Invalid output for server for echo! Received: " + out_line)
    client_proc.kill()
    server_proc.kill()


def main():
    if len(sys.argv) < 2:
        print("Usage: {} [path to bin folder]".format(sys.argv[0]))
        sys.exit(-1)
    bin_path = sys.argv[1]
    client_path = os.path.join(bin_path, "MessageClient.exe")
    server_path = os.path.join(bin_path, "MessageServer.exe")
    server_proc = ProcessInterface(server_path)
    client_proc = ProcessInterface(client_path)
    try:
        run_test(client_proc, server_proc)
        print("Test run successfully")
    except TestException as ex:
        print("Exception caught running echo test: " + str(ex))
        # Kill possibly running client/server
        client_proc.kill()
        server_proc.kill()


if __name__ == "__main__":
    main()

    #C:\Users\Skiry\AppData\Roaming\Microsoft\Windows\StartMenu\Programs\Python3.7 >
    # "Python 3.7 (32-bit).lnk"C:\Users\Skiry\Documents\Faculty\BitDefenderC\!Project2\BasicTester\test.py C:\Users\Skiry\Documents\Faculty\BitDefenderC\!Project2\MessageApp\bin\Win32\Debug\
