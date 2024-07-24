#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include "/home/mahmudul/ros_workspace/gnuplot-iostream/gnuplot-iostream.h"

int main() {
    std::string traj_csv_file_path = "/home/mahmudul/ros_workspace/src/tutorial_pkg/odomTrajData.csv";
    std::string vel_csv_file_path = "/home/mahmudul/ros_workspace/src/tutorial_pkg/odomVelData.csv";
    std::string laser_csv_file_path = "/home/mahmudul/ros_workspace/src/tutorial_pkg/laserData.csv";
    std::string laser_map_csv_file_path = "/home/mahmudul/ros_workspace/src/tutorial_pkg/laserMapData.csv";

    std::ifstream traj_file(traj_csv_file_path);
    std::ifstream vel_file(vel_csv_file_path);
    std::ifstream laser_file(laser_csv_file_path);
    std::ifstream laser_map_file(laser_map_csv_file_path);

    if (!traj_file.is_open()) {
        std::cerr << "Failed to open file: " << traj_csv_file_path << std::endl;
        return 1;
    }

    if (!vel_file.is_open()) {
        std::cerr << "Failed to open file: " << vel_csv_file_path << std::endl;
        return 1;
    }

    if (!laser_file.is_open()) {
        std::cerr << "Failed to open file: " << laser_csv_file_path << std::endl;
        return 1;
    }

    if (!laser_map_file.is_open()) {
        std::cerr << "Failed to open file: " << laser_map_csv_file_path << std::endl;
        return 1;
    }

    std::vector<std::pair<double, double>> traj_data;
    std::vector<std::pair<double, double>> vel_data;
    std::vector<std::pair<int, double>> laser_data;
    std::vector<std::pair<double, double>> laser_map_data;

    double posX, posY, time, velocity;
    int index;
    double range;

    while (traj_file >> posX >> posY) {
        traj_data.emplace_back(posX, posY);
    }

    while (vel_file >> time >> velocity) {
        vel_data.emplace_back(time, velocity);
    }

    while (laser_file >> index >> range) {
        laser_data.emplace_back(index, range);
    }

    while (laser_map_file >> posX >> posY) {
        laser_map_data.emplace_back(posX, posY);
    }

    traj_file.close();
    vel_file.close();
    laser_file.close();
    laser_map_file.close();

    Gnuplot gp;
    gp << "set xlabel 'Position X'\n";
    gp << "set ylabel 'Position Y'\n";
    gp << "set title 'Odometry Trajectory'\n";
    gp << "plot '-' with linespoints title 'Trajectory'\n";
    gp.send1d(traj_data);

    Gnuplot gp2;
    gp2 << "set xlabel 'Time (s)'\n";
    gp2 << "set ylabel 'Velocity (m/s)'\n";
    gp2 << "set title 'Velocity over Time'\n";
    gp2 << "plot '-' with linespoints title 'Velocity'\n";
    gp2.send1d(vel_data);

    Gnuplot gp3;
    gp3 << "set xlabel 'Index'\n";
    gp3 << "set ylabel 'Range (m)'\n";
    gp3 << "set title 'Laser Scan Data'\n";
    gp3 << "plot '-' with linespoints title 'Laser Data'\n";
    gp3.send1d(laser_data);

    Gnuplot gp4;
    gp4 << "set xlabel 'Global X'\n";
    gp4 << "set ylabel 'Global Y'\n";
    gp4 << "set title 'Transformed Laser Map Data'\n";
    gp4 << "plot '-' with points title 'Laser Map Data'\n";
    gp4.send1d(laser_map_data);

    return 0;
}
