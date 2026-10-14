package Transport;

import java.util.*;

public class RouteFinder {
    private Graph graph;

    public RouteFinder(Graph graph) {
        this.graph = graph;
    }

    // 查找最快路线
    public Path findFastestRoute(String start, String end, TransportType type) {
        PriorityQueue<Path> queue = new PriorityQueue<>(Comparator.comparingInt(p -> p.totalTime));
        queue.add(new Path(start, new ArrayList<>(), 0.0, 0));

        // 记录每个城市到达的最佳时间
        Map<String, Integer> bestTime = new HashMap<>();
        bestTime.put(start, 0);

        while (!queue.isEmpty()) {
            Path currentPath = queue.poll();
            String currentCity = currentPath.currentCity;

            if (currentCity.equals(end)) {
                return currentPath;
            }

            for (Transport transport : graph.getTransports(currentCity)) {
                if (transport.type != type) continue;

                // 计算等待时间
                int waitTime = 0;
                if (!currentPath.transports.isEmpty()) {
                    Transport lastTransport = currentPath.transports.get(currentPath.transports.size() - 1);
                    if (transport.departureTime < lastTransport.arrivalTime) {
                        // 行程跨日，等待时间为出发时间加一天减去上一个到达时间
                        waitTime = (transport.departureTime + 1440) - lastTransport.arrivalTime;
                    } else {
                        waitTime = transport.departureTime - lastTransport.arrivalTime;
                    }
                }

                // 计算行程时间
                int tripDuration = transport.arrivalTime - transport.departureTime;
                if (tripDuration < 0) {
                    tripDuration += 1440; // 跨日行程
                }

                int newTotalTime = currentPath.totalTime + tripDuration + waitTime;
                double newTotalCost = currentPath.totalCost + transport.cost;

                if (!bestTime.containsKey(transport.to) || newTotalTime < bestTime.get(transport.to)) {
                    bestTime.put(transport.to, newTotalTime);
                    List<Transport> newTransports = new ArrayList<>(currentPath.transports);
                    newTransports.add(transport);
                    queue.add(new Path(transport.to, newTransports, newTotalCost, newTotalTime));
                }
            }
        }

        return null; // 未找到路径
    }

    // 查找最省钱路线
    public Path findCheapestRoute(String start, String end, TransportType type) {
        PriorityQueue<Path> queue = new PriorityQueue<>(Comparator.comparingDouble(p -> p.totalCost));
        queue.add(new Path(start, new ArrayList<>(), 0.0, 0));

        // 记录每个城市到达的最佳费用
        Map<String, Double> bestCost = new HashMap<>();
        bestCost.put(start, 0.0);

        while (!queue.isEmpty()) {
            Path currentPath = queue.poll();
            String currentCity = currentPath.currentCity;

            if (currentCity.equals(end)) {
                return currentPath;
            }

            for (Transport transport : graph.getTransports(currentCity)) {
                if (transport.type != type) continue;

                // 计算等待时间
                int waitTime = 0;
                if (!currentPath.transports.isEmpty()) {
                    Transport lastTransport = currentPath.transports.get(currentPath.transports.size() - 1);
                    if (transport.departureTime < lastTransport.arrivalTime) {
                        // 行程跨日，等待时间为出发时间加一天减去上一个到达时间
                        waitTime = (transport.departureTime + 1440) - lastTransport.arrivalTime;
                    } else {
                        waitTime = transport.departureTime - lastTransport.arrivalTime;
                    }
                }

                // 计算行程时间
                int tripDuration = transport.arrivalTime - transport.departureTime;
                if (tripDuration < 0) {
                    tripDuration += 1440; // 跨日行程
                }

                int newTotalTime = currentPath.totalTime + tripDuration + waitTime;
                double newTotalCost = currentPath.totalCost + transport.cost;

                if (!bestCost.containsKey(transport.to) || newTotalCost < bestCost.get(transport.to)) {
                    bestCost.put(transport.to, newTotalCost);
                    List<Transport> newTransports = new ArrayList<>(currentPath.transports);
                    newTransports.add(transport);
                    queue.add(new Path(transport.to, newTransports, newTotalCost, newTotalTime));
                }
            }
        }

        return null; // 未找到路径
    }
}
