package Transport;

import java.util.*;

public class Path implements Comparable<Path> {
    String currentCity;
    List<Transport> transports;
    double totalCost;
    int totalTime;

    public Path(String currentCity, List<Transport> transports, double totalCost, int totalTime) {
        this.currentCity = currentCity;
        this.transports = transports;
        this.totalCost = totalCost;
        this.totalTime = totalTime;
    }

    @Override
    public int compareTo(Path other) {
        return Integer.compare(this.totalTime, other.totalTime);
    }
}
