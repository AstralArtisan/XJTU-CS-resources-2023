package Transport;

import java.util.*;
import java.io.*;

public class TrafficConsultationSystem {
    private static Graph graph = new Graph();
    private static Scanner scanner = new Scanner(System.in);

    public static void main(String[] args) {
        // 从文件加载数据
        loadDataFromFile("transport_schedule.txt");

        while (true) {
            printMenu();
            String choice = scanner.nextLine().trim();
            switch (choice) {
                case "1":
                    editCities();
                    break;
                case "2":
                    editTransports();
                    break;
                case "3":
                    queryRoute();
                    break;
                case "4":
                    System.out.println("退出系统。再见！");
                    return;
                default:
                    System.out.println("无效的选择，请重新输入。");
            }
        }
    }

    // 打印主菜单
    private static void printMenu() {
        System.out.println("\n--- 全国城市交通咨询系统 ---");
        System.out.println("1. 编辑城市信息");
        System.out.println("2. 编辑交通时刻表");
        System.out.println("3. 查询最优路线");
        System.out.println("4. 退出");
        System.out.print("请输入您的选择: ");
    }

    // 编辑城市信息
    private static void editCities() {
        while (true) {
            System.out.println("\n--- 编辑城市信息 ---");
            System.out.println("1. 添加城市");
            System.out.println("2. 删除城市");
            System.out.println("3. 列出所有城市");
            System.out.println("4. 返回主菜单");
            System.out.print("请输入您的选择: ");
            String choice = scanner.nextLine().trim();
            switch (choice) {
                case "1":
                    addCity();
                    break;
                case "2":
                    removeCity();
                    break;
                case "3":
                    listCities();
                    break;
                case "4":
                    return;
                default:
                    System.out.println("无效的选择，请重新输入。");
            }
        }
    }

    // 添加城市
    private static void addCity() {
        System.out.print("请输入要添加的城市名称: ");
        String city = scanner.nextLine().trim();
        if (graph.hasCity(city)) {
            System.out.println("该城市已存在。");
        } else {
            graph.addCity(city);
            System.out.println("城市添加成功。");
        }
    }

    // 删除城市
    private static void removeCity() {
        System.out.print("请输入要删除的城市名称: ");
        String city = scanner.nextLine().trim();
        if (!graph.hasCity(city)) {
            System.out.println("该城市不存在。");
        } else {
            graph.removeCity(city);
            System.out.println("城市删除成功。");
        }
    }

    // 列出所有城市
    private static void listCities() {
        System.out.println("当前所有城市列表:");
        for (String city : graph.getCities()) {
            System.out.println("- " + city);
        }
    }

    // 编辑交通时刻表
    private static void editTransports() {
        while (true) {
            System.out.println("\n--- 编辑交通时刻表 ---");
            System.out.println("1. 添加交通连接");
            System.out.println("2. 删除交通连接");
            System.out.println("3. 列出所有交通连接");
            System.out.println("4. 返回主菜单");
            System.out.print("请输入您的选择: ");
            String choice = scanner.nextLine().trim();
            switch (choice) {
                case "1":
                    addTransport();
                    break;
                case "2":
                    removeTransport();
                    break;
                case "3":
                    listTransports();
                    break;
                case "4":
                    return;
                default:
                    System.out.println("无效的选择，请重新输入。");
            }
        }
    }

    // 添加交通连接
    private static void addTransport() {
        try {
            System.out.print("请输入交通工具类型（火车/飞机）: ");
            String typeStr = scanner.nextLine().trim().toUpperCase();
            TransportType type = TransportType.valueOf(typeStr);

            System.out.print("请输入出发城市: ");
            String from = scanner.nextLine().trim();
            if (!graph.hasCity(from)) {
                System.out.println("出发城市不存在。");
                return;
            }

            System.out.print("请输入到达城市: ");
            String to = scanner.nextLine().trim();
            if (!graph.hasCity(to)) {
                System.out.println("到达城市不存在。");
                return;
            }

            System.out.print("请输入出发时间（HH:MM）: ");
            int departureTime = parseTime(scanner.nextLine().trim());
            System.out.print("请输入到达时间（HH:MM）: ");
            int arrivalTime = parseTime(scanner.nextLine().trim());

            if (arrivalTime < departureTime) {
                System.out.println("到达时间不能早于出发时间。");
                return;
            }

            System.out.print("请输入费用: ");
            double cost = Double.parseDouble(scanner.nextLine().trim());

            String trainID = "";
            if (type == TransportType.火车) {
                System.out.print("请输入火车编号: ");
                trainID = scanner.nextLine().trim();
                if (trainID.isEmpty()) {
                    System.out.println("火车编号不能为空。");
                    return;
                }
            }

            Transport transport = new Transport(from, to, type, departureTime, arrivalTime, cost, trainID.isEmpty() ? null : trainID);
            graph.addTransport(transport);
            System.out.println("交通连接添加成功。");
        } catch (IllegalArgumentException e) {
            System.out.println("输入的交通工具类型或时间格式有误。");
        }
    }

    // 删除交通连接
    private static void removeTransport() {
        try {
            System.out.print("请输入交通工具类型（火车/飞机）: ");
            String typeStr = scanner.nextLine().trim().toUpperCase();
            TransportType type = TransportType.valueOf(typeStr);

            System.out.print("请输入出发城市: ");
            String from = scanner.nextLine().trim();
            if (!graph.hasCity(from)) {
                System.out.println("出发城市不存在。");
                return;
            }

            System.out.print("请输入到达城市: ");
            String to = scanner.nextLine().trim();
            if (!graph.hasCity(to)) {
                System.out.println("到达城市不存在。");
                return;
            }

            System.out.print("请输入出发时间（HH:MM）: ");
            int departureTime = parseTime(scanner.nextLine().trim());
            System.out.print("请输入到达时间（HH:MM）: ");
            int arrivalTime = parseTime(scanner.nextLine().trim());

            System.out.print("请输入费用: ");
            double cost = Double.parseDouble(scanner.nextLine().trim());

            String trainID = "";
            if (type == TransportType.火车) {
                System.out.print("请输入火车编号: ");
                trainID = scanner.nextLine().trim();
                if (trainID.isEmpty()) {
                    System.out.println("火车编号不能为空。");
                    return;
                }
            }

            Transport transport = new Transport(from, to, type, departureTime, arrivalTime, cost, trainID.isEmpty() ? null : trainID);
            graph.removeTransport(transport);
            System.out.println("交通连接删除成功。");
        } catch (IllegalArgumentException e) {
            System.out.println("输入的交通工具类型或时间格式有误。");
        }
    }

    // 列出所有交通连接
    private static void listTransports() {
        System.out.println("当前所有交通连接列表:");
        for (String city : graph.getCities()) {
            for (Transport transport : graph.getTransports(city)) {
                String transportInfo = transport.type + ": " + transport.from + " -> " + transport.to +
                        ", 出发时间 " + formatTime(transport.departureTime) +
                        ", 到达时间 " + formatTime(transport.arrivalTime) +
                        ", 费用: " + transport.cost;
                if (transport.type == TransportType.火车 && transport.trainID != null) {
                    transportInfo += ", 火车编号: " + transport.trainID;
                }
                System.out.println(transportInfo);
            }
        }
    }

    // 查询最优路线
    private static void queryRoute() {
        System.out.print("请输入起始城市: ");
        String start = scanner.nextLine().trim();
        if (!graph.hasCity(start)) {
            System.out.println("起始城市不存在。");
            return;
        }

        System.out.print("请输入目的城市: ");
        String end = scanner.nextLine().trim();
        if (!graph.hasCity(end)) {
            System.out.println("目的城市不存在。");
            return;
        }

        System.out.print("请输入最优决策（1为最快/2为最便宜）: ");
        String decision = scanner.nextLine().trim().toUpperCase();
        if (!decision.equals("1") && !decision.equals("2")) {
            System.out.println("无效的最优决策。");
            return;
        }

        System.out.print("请输入交通工具类型（火车/飞机）: ");
        String typeStr = scanner.nextLine().trim().toUpperCase();
        TransportType type;
        try {
            type = TransportType.valueOf(typeStr);
        } catch (IllegalArgumentException e) {
            System.out.println("无效的交通工具类型。");
            return;
        }

        RouteFinder finder = new RouteFinder(graph);
        Path result = null;
        if (decision.equals("1")) {
            result = finder.findFastestRoute(start, end, type);
        } else if (decision.equals("2")) {
            result = finder.findCheapestRoute(start, end, type);
        }

        if (result == null) {
            System.out.println("根据给定的条件未找到合适的路线。");
        } else {
            if (decision.equals("1")) {
                System.out.println("最优路线需要 " + result.totalTime + " 分钟。");
            } else {
                System.out.println("最优路线费用为 " + result.totalCost + " 元。");
            }
            System.out.println("路线详情:");
            String currentTrainID = "";
            for (Transport t : result.transports) {
                if (t.type == TransportType.火车) {
                    if (!t.trainID.equals(currentTrainID)) {
                        currentTrainID = t.trainID;
                        System.out.println("火车编号: " + currentTrainID);
                    }
                    System.out.println("  火车: " + t.from + " -> " + t.to +
                            ", 出发时间 " + formatTime(t.departureTime) +
                            ", 到达时间 " + formatTime(t.arrivalTime) +
                            ", 费用: " + t.cost);
                } else {
                    System.out.println(t.type + ": " + t.from + " -> " + t.to +
                            ", 出发时间 " + formatTime(t.departureTime) +
                            ", 到达时间 " + formatTime(t.arrivalTime) +
                            ", 费用: " + t.cost);
                }
            }
        }
    }

    // 从文件加载数据
    private static void loadDataFromFile(String filename) {
        try (BufferedReader br = new BufferedReader(new FileReader(filename))) {
            String line;
            boolean isCitiesSection = false;
            boolean isTransportsSection = false;
            while ((line = br.readLine()) != null) {
                line = line.trim();
                if (line.isEmpty()) continue;
                if (line.startsWith("#")) {
                    if (line.equalsIgnoreCase("# Cities")) {
                        isCitiesSection = true;
                        isTransportsSection = false;
                    } else if (line.equalsIgnoreCase("# Transports")) {
                        isCitiesSection = false;
                        isTransportsSection = true;
                    } else {
                        isCitiesSection = false;
                        isTransportsSection = false;
                    }
                    continue;
                }

                if (isCitiesSection) {
                    graph.addCity(line);
                } else if (isTransportsSection) {
                    String[] parts = line.split(",");
                    if (parts.length < 6) {
                        System.out.println("无效的交通连接格式: " + line);
                        continue;
                    }
                    String typeStr = parts[0].trim().toUpperCase();
                    TransportType type;
                    try {
                        type = TransportType.valueOf(typeStr);
                    } catch (IllegalArgumentException e) {
                        System.out.println("无效的交通工具类型: " + typeStr);
                        continue;
                    }
                    String from = parts[1].trim();
                    String to = parts[2].trim();
                    int departureTime = parseTime(parts[3].trim());
                    int arrivalTime = parseTime(parts[4].trim());
                    double cost = Double.parseDouble(parts[5].trim());
                    String trainID = null;
                    if (type == TransportType.火车) {
                        if (parts.length >= 7) {
                            trainID = parts[6].trim();
                            if (trainID.isEmpty()) {
                                trainID = null;
                            }
                        }
                    }
                    Transport transport = new Transport(from, to, type, departureTime, arrivalTime, cost, trainID);
                    graph.addTransport(transport);
                }
            }
            System.out.println("数据加载完成。");
        } catch (FileNotFoundException e) {
            System.out.println("文件未找到: " + filename);
        } catch (IOException e) {
            System.out.println("读取文件时发生错误: " + e.getMessage());
        }
    }

    // 将HH:MM格式的时间转换为分钟数
    private static int parseTime(String timeStr) {
        String[] parts = timeStr.split(":");
        if (parts.length != 2) throw new IllegalArgumentException("时间格式错误。");
        int hours = Integer.parseInt(parts[0]);
        int minutes = Integer.parseInt(parts[1]);
        return hours * 60 + minutes;
    }

    // 将分钟数格式化为HH:MM格式
    private static String formatTime(int time) {
        int hours = (time / 60) % 24;
        int minutes = time % 60;
        return String.format("%02d:%02d", hours, minutes);
    }
}
