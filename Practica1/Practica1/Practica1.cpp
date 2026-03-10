#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <future>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <stdexcept>
#include <optional>
#include <memory>

using namespace std;

struct Zona {
    string nombre;
    int largoCm = 0;
    int anchoCm = 0;
    long long areaCm2 = 0;
    sf::RectangleShape forma;
    unique_ptr<sf::Text> texto;
};

struct Nodo {
    string nombre;
    sf::Vector2f posicion;
    sf::CircleShape forma;
};

long long calcularArea(int largo, int ancho) {
    if (largo <= 0 || ancho <= 0)
        throw invalid_argument("Dimensiones incorrectas");

    return static_cast<long long>(largo) * ancho;
}

string formatearDecimal(double valor, int decimales = 2) {
    stringstream ss;
    ss << fixed << setprecision(decimales) << valor;
    return ss.str();
}

float distancia(const sf::Vector2f& a, const sf::Vector2f& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return sqrt(dx * dx + dy * dy);
}

sf::Vector2f normalizar(const sf::Vector2f& v) {
    float m = sqrt(v.x * v.x + v.y * v.y);
    if (m == 0.f) return { 0.f, 0.f };
    return { v.x / m, v.y / m };
}

int main() {
    try {
        sf::RenderWindow ventana(sf::VideoMode({ 1100u,700u }), "Robot Aspirador");
        ventana.setFramerateLimit(60);

        sf::Font fuente;
        if (!fuente.openFromFile("dogica.ttf")) {
            throw runtime_error("Error al cargar la fuente dogica.ttf");
        }

        const float escala = 0.8f;
        const float offsetX = 60.f;
        const float offsetY = 60.f;

        const float habitacionAnchoPx = 500.f * escala;
        const float habitacionAltoPx = 630.f * escala;

        sf::RectangleShape fondoHabitacion({ habitacionAnchoPx, habitacionAltoPx });
        fondoHabitacion.setPosition({ offsetX, offsetY });
        fondoHabitacion.setFillColor(sf::Color(235, 235, 235));
        fondoHabitacion.setOutlineThickness(8.f);
        fondoHabitacion.setOutlineColor(sf::Color(90, 90, 90));

        sf::RectangleShape mueble({ 90.f * escala,260.f * escala });
        mueble.setPosition({ offsetX + 101.f * escala, offsetY + 150.f * escala });
        mueble.setFillColor(sf::Color(120, 80, 40));
        mueble.setOutlineThickness(2.f);
        mueble.setOutlineColor(sf::Color::Black);

        vector<Zona> zonas(4);

        zonas[0].nombre = "Zona 1";
        zonas[0].largoCm = 500;
        zonas[0].anchoCm = 150;
        zonas[0].forma.setSize({ 500.f * escala,150.f * escala });
        zonas[0].forma.setPosition({ offsetX,offsetY });
        zonas[0].forma.setFillColor(sf::Color(255, 200, 200, 180));
        zonas[0].forma.setOutlineThickness(2.f);
        zonas[0].forma.setOutlineColor(sf::Color::Red);

        zonas[1].nombre = "Zona 2";
        zonas[1].largoCm = 480;
        zonas[1].anchoCm = 101;
        zonas[1].forma.setSize({ 101.f * escala,480.f * escala });
        zonas[1].forma.setPosition({ offsetX,offsetY + 150.f * escala });
        zonas[1].forma.setFillColor(sf::Color(200, 255, 200, 180));
        zonas[1].forma.setOutlineThickness(2.f);
        zonas[1].forma.setOutlineColor(sf::Color::Green);

        zonas[2].nombre = "Zona 3";
        zonas[2].largoCm = 309;
        zonas[2].anchoCm = 480;
        zonas[2].forma.setSize({ 309.f * escala,480.f * escala });
        zonas[2].forma.setPosition({ offsetX + 191.f * escala,offsetY + 150.f * escala });
        zonas[2].forma.setFillColor(sf::Color(200, 200, 255, 180));
        zonas[2].forma.setOutlineThickness(2.f);
        zonas[2].forma.setOutlineColor(sf::Color::Blue);

        zonas[3].nombre = "Zona 4";
        zonas[3].largoCm = 90;
        zonas[3].anchoCm = 220;
        zonas[3].forma.setSize({ 90.f * escala,220.f * escala });
        zonas[3].forma.setPosition({ offsetX + 101.f * escala,offsetY + 410.f * escala });
        zonas[3].forma.setFillColor(sf::Color(255, 255, 180, 180));
        zonas[3].forma.setOutlineThickness(2.f);
        zonas[3].forma.setOutlineColor(sf::Color(180, 140, 0));

        vector<future<long long>> tareas;

        for (const auto& z : zonas) {
            tareas.push_back(async(launch::async, calcularArea, z.largoCm, z.anchoCm));
        }

        long long superficieTotal = 0;

        for (size_t i = 0; i < zonas.size(); i++) {
            zonas[i].areaCm2 = tareas[i].get();
            superficieTotal += zonas[i].areaCm2;
        }

        double superficieTotalM2 = static_cast<double>(superficieTotal) / 10000.0;
        double tasaLimpieza = 1000.0;
        double tiempoSegundos = superficieTotal / tasaLimpieza;
        int minutos = static_cast<int>(tiempoSegundos) / 60;
        int segundos = static_cast<int>(tiempoSegundos) % 60;

        cout << "Superficie total: " << superficieTotal << " cm2" << endl;
        cout << "Tiempo estimado: " << tiempoSegundos << " s" << endl;

        for (auto& z : zonas) {
            string textoZona = z.nombre + "\n\n" + to_string(z.areaCm2) + " cm2";
            z.texto = make_unique<sf::Text>(fuente, textoZona, 12);
            z.texto->setFillColor(sf::Color::Black);

            sf::FloatRect caja = z.forma.getGlobalBounds();
            z.texto->setPosition({ caja.position.x + 8.f, caja.position.y + 8.f });
        }

        sf::Text titulo(fuente, "Simulacion de limpieza", 16);
        titulo.setFillColor(sf::Color::Black);
        titulo.setPosition({ 650.f, 40.f });

        stringstream info;
        info << "Resultados:\n\n";
        for (const auto& z : zonas) {
            info << z.nombre << ":\n\n" << z.areaCm2 << " cm2\n\n";
        }
        info << "Superficie total: " << superficieTotal << " cm2\n\n";
        info << "Superficie total: " << formatearDecimal(superficieTotalM2) << " m2\n\n";
        info << "Tasa limpieza: " << tasaLimpieza << " cm2/s\n\n";
        info << "Tiempo estimado: " << formatearDecimal(tiempoSegundos) << " s\n\n";
        info << "Tiempo aprox: " << minutos << " min " << segundos << " s\n\n";
        info << "Controles:\n\n";
        info << "R -> reiniciar robot\n\n";
        info << "ESC -> salir";

        sf::Text textoInfo(fuente, info.str(), 12);
        textoInfo.setFillColor(sf::Color::Black);
        textoInfo.setPosition({ 650.f, 90.f });

        vector<Nodo> nodos;

        auto crearNodo = [&](string nombre, float x, float y) {
            Nodo n;
            n.nombre = nombre;
            n.posicion = { offsetX + x * escala,offsetY + y * escala };

            n.forma = sf::CircleShape(8.f);
            n.forma.setOrigin({ 8.f,8.f });
            n.forma.setPosition(n.posicion);
            n.forma.setFillColor(sf::Color::Red);
            n.forma.setOutlineColor(sf::Color::Black);
            n.forma.setOutlineThickness(1.f);

            nodos.push_back(n);
            };

        crearNodo("A", 101, 150);
        crearNodo("B", 191, 150);
        crearNodo("C", 101, 260);
        crearNodo("D", 191, 260);
        crearNodo("E", 500, 260);
        crearNodo("F", 101, 630);
        crearNodo("G", 191, 630);

        vector<pair<int, int>> conexiones = {
            {0,1},{0,2},{1,3},{2,3},
            {2,5},{3,4},{5,6},{4,6}
        };

        sf::CircleShape robot(10.f);
        robot.setOrigin({ 10.f,10.f });
        robot.setFillColor(sf::Color::Blue);
        robot.setOutlineThickness(2.f);
        robot.setOutlineColor(sf::Color::Black);

        vector<sf::Vector2f> ruta = {
            nodos[0].posicion,
            nodos[1].posicion,
            nodos[3].posicion,
            nodos[4].posicion,
            nodos[6].posicion,
            nodos[5].posicion,
            nodos[2].posicion,
            nodos[0].posicion
        };

        size_t objetivo = 1;
        sf::Vector2f posRobot = ruta[0];
        robot.setPosition(posRobot);

        float velocidad = 80.f;

        sf::Clock reloj;

        while (ventana.isOpen()) {
            float dt = reloj.restart().asSeconds();

            while (auto evento = ventana.pollEvent()) {
                if (evento->is<sf::Event::Closed>()) {
                    ventana.close();
                }
            }

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape)) {
                ventana.close();
            }

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::R)) {
                posRobot = ruta[0];
                objetivo = 1;
            }

            if (objetivo < ruta.size()) {
                sf::Vector2f destino = ruta[objetivo];
                sf::Vector2f dir = destino - posRobot;

                float dist = distancia(posRobot, destino);

                if (dist < 2.f) {
                    posRobot = destino;
                    objetivo++;

                    if (objetivo >= ruta.size()) {
                        objetivo = 1;
                        posRobot = ruta[0];
                    }
                }
                else {
                    posRobot += normalizar(dir) * velocidad * dt;
                }
            }

            robot.setPosition(posRobot);

            ventana.clear(sf::Color(210, 210, 210));

            sf::RectangleShape panel({ 390.f, 620.f });
            panel.setPosition({ 640.f, 30.f });
            panel.setFillColor(sf::Color(245, 245, 245));
            panel.setOutlineThickness(2.f);
            panel.setOutlineColor(sf::Color(120, 120, 120));

            ventana.draw(fondoHabitacion);

            for (auto& z : zonas) {
                ventana.draw(z.forma);
            }

            ventana.draw(mueble);

            for (auto& c : conexiones) {
                sf::Vertex linea[] = {
                    sf::Vertex{nodos[c.first].posicion,sf::Color::White},
                    sf::Vertex{nodos[c.second].posicion,sf::Color::White}
                };
                ventana.draw(linea, 2, sf::PrimitiveType::Lines);
            }

            for (auto& n : nodos) {
                ventana.draw(n.forma);

                sf::Text nombreNodo(fuente, n.nombre, 10);
                nombreNodo.setFillColor(sf::Color::Black);
                nombreNodo.setPosition({ n.posicion.x + 10.f, n.posicion.y - 10.f });
                ventana.draw(nombreNodo);
            }

            for (const auto& z : zonas) {
                if (z.texto) {
                    ventana.draw(*z.texto);
                }
            }

            ventana.draw(robot);

            ventana.draw(panel);
            ventana.draw(titulo);
            ventana.draw(textoInfo);

            ventana.display();
        }
    }
    catch (const exception& e) {
        cout << "Error: " << e.what() << endl;
        return -1;
    }

    return 0;
}