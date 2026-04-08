#include "models.h"
#include "repository.h"
#include "event_bus.h"
#include "services.h"
#include "visualizer.h"

#include <iostream>
#include <thread>
#include <ctime>
#include <iomanip>
#include <sstream>

std::string currentDateTime()
{
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);

    std::ostringstream oss;
    oss << std::put_time(localTime, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

int main()
{
    Repository repository;
    EventBus eventBus;

    // =========================
    // MODELO: ZONAS DE LIMPIEZA
    // =========================
    // Coordenadas visuales ajustadas a SFML según el plano.
    repository.addZone({ 1, "Zona 1", 180, 80, 500, 150, 500, 150 });
    repository.addZone({ 2, "Zona 2", 180, 230, 100, 480, 480, 101 });
    repository.addZone({ 3, "Zona 3", 380, 230, 300, 480, 309, 480 });
    repository.addZone({ 4, "Zona 4", 280, 490, 100, 220, 90, 220 });

    // =========================
    // MODELO: ROOMBAS
    // =========================
    repository.addRoomba({ 1, "Roomba-A", 1000.0f, false });
    repository.addRoomba({ 2, "Roomba-B", 1200.0f, false });

    // =========================
    // SESION
    // =========================
    CleaningSession session;
    session.sessionId = 1;
    session.startTime = currentDateTime();
    session.roombasUsed = static_cast<int>(repository.getRoombas().size());
    session.totalArea = repository.calculateTotalArea();

    // Tiempo estimado global aproximado con tasa media
    float avgRate = 1100.0f;
    session.estimatedTime = session.totalArea / avgRate;

    repository.addSession(session);

    // =========================
    // SERVICIOS DISTRIBUIDOS
    // =========================
    ClientServerService clientServer(repository, eventBus);
    PeerToPeerService p2pService;
    CleaningCoordinator coordinator(repository, eventBus, clientServer, p2pService);

    // =========================
    // VISTA SFML
    // =========================
    Visualizer visualizer(repository, eventBus);

    // Arrancamos la limpieza en un hilo aparte
    std::thread cleaningThread([&coordinator]() {
        coordinator.startCleaning();
        coordinator.waitForCompletion();
        });

    // Mostramos consola con cálculos
    std::cout << "===== CALCULO DE AREAS =====\n";
    for (const auto& z : repository.getZones())
    {
        std::cout << z.name << ": " << z.largoCm << " x " << z.anchoCm
            << " = " << z.area() << " cm2\n";
    }

    std::cout << "\nSuperficie total a limpiar: "
        << repository.calculateTotalArea() << " cm2\n";

    std::cout << "Tiempo estimado de limpieza: "
        << session.estimatedTime << " segundos aprox.\n";

    // Bucle visual
    visualizer.processEvents();

    if (cleaningThread.joinable())
        cleaningThread.join();

    return 0;
}