package Transport;

import java.util.*;

public class Graph {
    private Map<String, List<Transport>> adjList;
    public Graph() {
        adjList = new HashMap<>();
    }

    public void addCity(String city) {
        adjList.putIfAbsent(city, new ArrayList<>());
    }

    public void removeCity(String city) {
        adjList.remove(city);
        for (List<Transport> transports : adjList.values()) {
            transports.removeIf(t -> t.to.equals(city));
        }
    }

    public void addTransport(Transport transport) {
        adjList.putIfAbsent(transport.from, new ArrayList<>());
        adjList.get(transport.from).add(transport);
    }

    public void removeTransport(Transport transport) {
        if (adjList.containsKey(transport.from)) {
            adjList.get(transport.from).removeIf(t ->
                    t.to.equals(transport.to) &&
                            t.type == transport.type &&
                            t.departureTime == transport.departureTime &&
                            t.arrivalTime == transport.arrivalTime &&
                            t.cost == transport.cost &&
                            ((t.trainID == null && transport.trainID == null) ||
                                    (t.trainID != null && t.trainID.equals(transport.trainID)))
            );
        }
    }

    public List<Transport> getTransports(String city) {
        return adjList.getOrDefault(city, new ArrayList<>());
    }

    public boolean hasCity(String city) {
        return adjList.containsKey(city);
    }

    public Set<String> getCities() {
        return adjList.keySet();
    }
}
