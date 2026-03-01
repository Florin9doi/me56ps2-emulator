#pragma once

#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <vector>

class ISP {
public:
    static void runCommand(const std::vector<std::string>& args) {
        if (args.empty()) return;

        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "Failed to fork for command\n";
            return;
        }

        if (pid == 0) {
            // Child process
            std::vector<char*> c_args;
            for (const auto& s : args)
                c_args.push_back(const_cast<char*>(s.c_str()));
            c_args.push_back(nullptr);

            // Detach signals to avoid EINTR on parent
            setsid();

            execvp(c_args[0], c_args.data());
            std::cerr << "Failed to exec " << c_args[0] << "\n";
            _exit(1);
        } else {
            // Parent process: wait for child
            int status;
            waitpid(pid, &status, 0);
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                std::cerr << "Command failed: " << args[0] << "\n";
            }
        }
    }

    static void setIpforward() {
        runCommand({"sudo", "sysctl", "-w", "net.ipv4.ip_forward=1"});
        std::cout << "IP forwarding enabled\n";
    }

    static void setIptables() {
        runCommand({"sudo", "sh", "-c",
            "(iptables -t nat -C POSTROUTING -o wlan0 -j MASQUERADE 2>/dev/null ||"
            " iptables -t nat -A POSTROUTING -o wlan0 -j MASQUERADE)"});
        runCommand({"sudo", "sh", "-c",
            "(iptables -C FORWARD -i ppp+ -o wlan0 -j ACCEPT 2>/dev/null ||"
            " iptables -A FORWARD -i ppp+ -o wlan0 -j ACCEPT)"});
        runCommand({"sudo", "sh", "-c",
            "(iptables -C FORWARD -i wlan0 -o ppp+ -m state --state RELATED,ESTABLISHED -j ACCEPT 2>/dev/null ||"
            " iptables -A FORWARD -i wlan0 -o ppp+ -m state --state RELATED,ESTABLISHED -j ACCEPT)"});
        std::cout << "iptables rules applied successfully\n";
    }

    static void startPPP(std::string slave_name) {
        if (slave_name.empty()) {
            std::cerr << "Slave name not set!\n";
            return;
        }

        std::vector<std::string> cmd = {
            "sudo",
            "pppd",
            slave_name,
            "115200",
            "local",
            "debug",
            "10.0.0.1:10.0.0.2",
            "ms-dns", "8.8.8.8",
            "proxyarp"
        };

        runCommand(cmd);
    }

    static void setupISP(std::string slave_name) {
        setIpforward();
        setIptables();
        startPPP(slave_name);
    }
};
