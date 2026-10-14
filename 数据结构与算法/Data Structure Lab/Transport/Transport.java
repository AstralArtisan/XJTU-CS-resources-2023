package Transport;

public class Transport {
    String from;
    String to;
    TransportType type;
    int departureTime;
    int arrivalTime;
    double cost;
    String trainID;

    public Transport(String from, String to, TransportType type, int departureTime, int arrivalTime, double cost, String TrainID) {
        this.from = from;
        this.to = to;
        this.type = type;
        this.departureTime = departureTime;
        this.arrivalTime = arrivalTime;
        this.cost = cost;
        this.trainID = TrainID;
    }
}
