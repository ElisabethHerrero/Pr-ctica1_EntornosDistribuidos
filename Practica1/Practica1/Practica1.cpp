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
#include <fstream>
#include <map>
#include <algorithm>

using namespace std;

enum class Direccion {
    UP,
    DOWN,
    LEFT,
    RIGHT
};

struct Zona {
    string nombre;
    int largoCm = 0;
    int anchoCm = 0;
    long long areaCm2 = 0;
    sf::RectangleShape forma;
    unique_ptr<sf::Text> texto;
};

struct Nodo {
    int x = 0;
    int y = 0;
    sf::CircleShape forma;
    map<Direccion, Nodo*> vecinos;

    Nodo(int px, int py) : x(px), y(py) {
        forma = sf::CircleShape(8.f);
        forma.setOrigin({ 8.f, 8.f });
        forma.setPosition({ static_cast<float>(x), static_cast<float>(y) });
        forma.setFillColor(sf::Color::Red);
        forma.setOutlineColor(sf::Color::Black);
        forma.setOutlineThickness(1.f);

        vecinos[Direccion::UP] = nullptr;
        vecinos[Direccion::DOWN] = nullptr;
        vecinos[Direccion::LEFT] = nullptr;
        vecinos[Direccion::RIGHT] = nullptr;
    }

    sf::Vector2f getPosicion() const {
        return { static_cast<float>(x), static_cast<float>(y) };
    }

    void render(sf::RenderWindow& ventana) {
        for (const auto& par : vecinos) {
            if (par.second != nullptr) {
                sf::Vertex linea[] = {
                    sf::Vertex{getPosicion(), sf::Color::White},
                    sf::Vertex{par.second->getPosicion(), sf::Color::White}
                };
                ventana.draw(linea, 2, sf::PrimitiveType::Lines);
            }
        }
        ventana.draw(forma);
    }
};

class NodeGroup {
private:
    vector<vector<char>> data;
    map<pair<int, int>, unique_ptr<Nodo>> nodesLUT;
    vector<char> nodeSymbols = { '+' };
    vector<char> pathSymbols = { '.' };

    int tileWidth;
    int tileHeight;
    int xOffset;
    int yOffset;

    bool contiene(const vector<char>& lista, char c) const {
        return find(lista.begin(), lista.end(), c) != lista.end();
    }

public:
    NodeGroup(const string& level, int tileW, int tileH, int offX, int offY)
        : tileWidth(tileW), tileHeight(tileH), xOffset(offX), yOffset(offY) {
        data = readMazeFile(level);
        createNodeTable(data);
        connectHorizontally(data);
        connectVertically(data);
    }

    vector<vector<char>> readMazeFile(const string& textfile) {
        ifstream archivo(textfile);
        if (!archivo.is_open()) {
            throw runtime_error("Error al abrir el archivo del laberinto: " + textfile);
        }

        vector<vector<char>> matriz;
        string linea;

        while (getline(archivo, linea)) {
            stringstream ss(linea);
            vector<char> fila;
            char simbolo;

            while (ss >> simbolo) {
                fila.push_back(simbolo);
            }

            if (!fila.empty()) {
                matriz.push_back(fila);
            }
        }

        if (matriz.empty()) {
            throw runtime_error("El archivo del laberinto esta vacio.");
        }

        return matriz;
    }

    pair<int, int> constructKey(int col, int row) const {
        return {
            xOffset + col * tileWidth,
            yOffset + row * tileHeight
        };
    }

    void createNodeTable(const vector<vector<char>>& data) {
        for (int row = 0; row < static_cast<int>(data.size()); row++) {
            for (int col = 0; col < static_cast<int>(data[row].size()); col++) {
                if (contiene(nodeSymbols, data[row][col])) {
                    pair<int, int> key = constructKey(col, row);

                    if (nodesLUT.find(key) == nodesLUT.end()) {
                        nodesLUT[key] = make_unique<Nodo>(key.first, key.second);
                    }
                }
            }
        }
    }

    void connectHorizontally(const vector<vector<char>>& data) {
        for (int row = 0; row < static_cast<int>(data.size()); row++) {
            optional<pair<int, int>> key;

            for (int col = 0; col < static_cast<int>(data[row].size()); col++) {
                char actual = data[row][col];

                if (contiene(nodeSymbols, actual)) {
                    pair<int, int> otherKey = constructKey(col, row);

                    if (!key.has_value()) {
                        key = otherKey;
                    }
                    else {
                        nodesLUT[*key]->vecinos[Direccion::RIGHT] = nodesLUT[otherKey].get();
                        nodesLUT[otherKey]->vecinos[Direccion::LEFT] = nodesLUT[*key].get();
                        key = otherKey;
                    }
                }
                else if (!contiene(pathSymbols, actual)) {
                    key.reset();
                }
            }
        }
    }

    void connectVertically(const vector<vector<char>>& data) {
        int filas = static_cast<int>(data.size());
        int columnas = static_cast<int>(data[0].size());

        for (int col = 0; col < columnas; col++) {
            optional<pair<int, int>> key;

            for (int row = 0; row < filas; row++) {
                char actual = data[row][col];

                if (contiene(nodeSymbols, actual)) {
                    pair<int, int> otherKey = constructKey(col, row);

                    if (!key.has_value()) {
                        key = otherKey;
                    }
                    else {
                        nodesLUT[*key]->vecinos[Direccion::DOWN] = nodesLUT[otherKey].get();
                        nodesLUT[otherKey]->vecinos[Direccion::UP] = nodesLUT[*key].get();
                        key = otherKey;
                    }
                }
                else if (!contiene(pathSymbols, actual)) {
                    key.reset();
                }
            }
        }
    }

    Nodo* getNodeFromPixels(int xpixel, int ypixel) {
        pair<int, int> key = { xpixel, ypixel };
        if (nodesLUT.find(key) != nodesLUT.end()) {
            return nodesLUT[key].get();
        }
        return nullptr;
    }

    Nodo* getNodeFromTiles(int col, int row) {
        pair<int, int> key = constructKey(col, row);
        if (nodesLUT.find(key) != nodesLUT.end()) {
            return nodesLUT[key].get();
        }
        return nullptr;
    }

    Nodo* getStartTempNode() {
        if (nodesLUT.empty()) return nullptr;
        return nodesLUT.begin()->second.get();
    }

    void render(sf::RenderWindow& ventana) {
        for (auto& par : nodesLUT) {
            par.second->render(ventana);
        }
    }
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

Direccion direccionOpuesta(Direccion d) {
    if (d == Direccion::UP) return Direccion::DOWN;
    if (d == Direccion::DOWN) return Direccion::UP;
    if (d == Direccion::LEFT) return Direccion::RIGHT;
    return Direccion::LEFT;
}

Direccion elegirSiguienteDireccion(Nodo* nodoActual, Direccion direccionActual) {
    if (nodoActual == nullptr) return Direccion::RIGHT;

    if (nodoActual->vecinos[direccionActual] != nullptr) {
        return direccionActual;
    }

    vector<Direccion> prioridad = {
        Direccion::RIGHT,
        Direccion::DOWN,
        Direccion::LEFT,
        Direccion::UP
    };

    for (Direccion d : prioridad) {
        if (d != direccionOpuesta(direccionActual) && nodoActual->vecinos[d] != nullptr) {
            return d;
        }
    }

    for (Direccion d : prioridad) {
        if (nodoActual->vecinos[d] != nullptr) {
            return d;
        }
    }

    return direccionActual;
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
        info << "Laberinto:\n\n";
        info << "+ = nodo\n\n";
        info << ". = camino\n\n";
        info << "X = vacio\n\n";
        info << "Controles:\n\n";
        info << "R -> reiniciar robot\n\n";
        info << "ESC -> salir";

        sf::Text textoInfo(fuente, info.str(), 12);
        textoInfo.setFillColor(sf::Color::Black);
        textoInfo.setPosition({ 650.f, 90.f });

        // NIVEL 2 APLICADO
        NodeGroup nodes("mazetest.txt", 45, 45, static_cast<int>(offsetX + 30), static_cast<int>(offsetY + 30));

        Nodo* nodoActual = nodes.getStartTempNode();
        if (nodoActual == nullptr) {
            throw runtime_error("No hay nodos en el laberinto.");
        }

        Direccion direccionActual = Direccion::RIGHT;
        Nodo* nodoObjetivo = nodoActual->vecinos[direccionActual];

        if (nodoObjetivo == nullptr) {
            direccionActual = elegirSiguienteDireccion(nodoActual, direccionActual);
            nodoObjetivo = nodoActual->vecinos[direccionActual];
        }

        sf::CircleShape robot(10.f);
        robot.setOrigin({ 10.f,10.f });
        robot.setFillColor(sf::Color::Yellow);
        robot.setOutlineThickness(2.f);
        robot.setOutlineColor(sf::Color::Black);

        sf::Vector2f posRobot = nodoActual->getPosicion();
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
                nodoActual = nodes.getStartTempNode();
                direccionActual = Direccion::RIGHT;
                nodoObjetivo = nodoActual->vecinos[direccionActual];

                if (nodoObjetivo == nullptr) {
                    direccionActual = elegirSiguienteDireccion(nodoActual, direccionActual);
                    nodoObjetivo = nodoActual->vecinos[direccionActual];
                }

                posRobot = nodoActual->getPosicion();
            }

            if (nodoObjetivo != nullptr) {
                sf::Vector2f destino = nodoObjetivo->getPosicion();
                sf::Vector2f dir = destino - posRobot;
                float dist = distancia(posRobot, destino);

                if (dist < 2.f) {
                    posRobot = destino;
                    nodoActual = nodoObjetivo;
                    direccionActual = elegirSiguienteDireccion(nodoActual, direccionActual);
                    nodoObjetivo = nodoActual->vecinos[direccionActual];
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

            nodes.render(ventana);

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