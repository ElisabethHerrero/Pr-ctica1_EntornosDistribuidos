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
#include <ctime>
#include <cstdlib>

using namespace std;

enum class Direccion {
    STOP,
    UP,
    DOWN,
    LEFT,
    RIGHT
};

Direccion direccionOpuesta(Direccion d) {
    if (d == Direccion::UP) return Direccion::DOWN;
    if (d == Direccion::DOWN) return Direccion::UP;
    if (d == Direccion::LEFT) return Direccion::RIGHT;
    if (d == Direccion::RIGHT) return Direccion::LEFT;
    return Direccion::STOP;
}

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
        // Dibujamos solo RIGHT y DOWN para no duplicar líneas
        if (vecinos.at(Direccion::RIGHT) != nullptr) {
            sf::Vertex linea[] = {
                sf::Vertex{getPosicion(), sf::Color::White},
                sf::Vertex{vecinos.at(Direccion::RIGHT)->getPosicion(), sf::Color::White}
            };
            ventana.draw(linea, 2, sf::PrimitiveType::Lines);
        }

        if (vecinos.at(Direccion::DOWN) != nullptr) {
            sf::Vertex linea[] = {
                sf::Vertex{getPosicion(), sf::Color::White},
                sf::Vertex{vecinos.at(Direccion::DOWN)->getPosicion(), sf::Color::White}
            };
            ventana.draw(linea, 2, sf::PrimitiveType::Lines);
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

    Nodo* getNthNode(size_t index) {
        if (index >= nodesLUT.size()) return nullptr;
        auto it = nodesLUT.begin();
        advance(it, index);
        return it->second.get();
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

class Entity {
protected:
    string nombre;
    map<Direccion, sf::Vector2f> directions;
    Direccion direction;
    float speed;
    float radius;
    sf::Color color;
    Nodo* node;
    Nodo* target;
    sf::Vector2f position;
    bool visible;

public:
    Entity(Nodo* nodoInicial, int tileWidth) {
        nombre = "Entity";
        directions[Direccion::UP] = sf::Vector2f(0.f, -1.f);
        directions[Direccion::DOWN] = sf::Vector2f(0.f, 1.f);
        directions[Direccion::LEFT] = sf::Vector2f(-1.f, 0.f);
        directions[Direccion::RIGHT] = sf::Vector2f(1.f, 0.f);
        directions[Direccion::STOP] = sf::Vector2f(0.f, 0.f);

        direction = Direccion::STOP;
        setSpeed(100.f, tileWidth);
        radius = 10.f;
        color = sf::Color::White;
        node = nodoInicial;
        target = nodoInicial;
        visible = true;
        setPosition();
    }

    virtual ~Entity() = default;

    void setPosition() {
        if (node != nullptr) {
            position = node->getPosicion();
        }
    }

    bool validDirection(Direccion dir) {
        if (dir != Direccion::STOP) {
            if (node->vecinos[dir] != nullptr) {
                return true;
            }
        }
        return false;
    }

    Nodo* getNewTarget(Direccion dir) {
        if (validDirection(dir)) {
            return node->vecinos[dir];
        }
        return node;
    }

    bool overshotTarget() {
        if (target != nullptr) {
            sf::Vector2f vec1 = target->getPosicion() - node->getPosicion();
            sf::Vector2f vec2 = position - node->getPosicion();

            float nodo2Target = vec1.x * vec1.x + vec1.y * vec1.y;
            float nodo2Self = vec2.x * vec2.x + vec2.y * vec2.y;

            return nodo2Self >= nodo2Target;
        }
        return false;
    }

    void reverseDirection() {
        direction = direccionOpuesta(direction);
        Nodo* temp = node;
        node = target;
        target = temp;
    }

    void setSpeed(float velocidadBase, int tileWidth) {
        speed = velocidadBase * static_cast<float>(tileWidth) / 16.f;
    }

    vector<Direccion> validDirections() {
        vector<Direccion> dirs;

        for (Direccion dir : {Direccion::UP, Direccion::DOWN, Direccion::LEFT, Direccion::RIGHT}) {
            if (validDirection(dir)) {
                if (dir != direccionOpuesta(direction)) {
                    dirs.push_back(dir);
                }
            }
        }

        if (dirs.empty()) {
            dirs.push_back(direccionOpuesta(direction));
        }

        return dirs;
    }

    Direccion randomDirection(const vector<Direccion>& dirs) {
        int index = rand() % dirs.size();
        return dirs[index];
    }

    virtual void update(float dt) {
        position += directions[direction] * speed * dt;

        if (overshotTarget()) {
            node = target;

            vector<Direccion> dirs = validDirections();
            Direccion dir = randomDirection(dirs);

            target = getNewTarget(dir);
            if (target != node) {
                direction = dir;
            }
            else {
                target = getNewTarget(direction);
            }

            setPosition();
        }
    }

    virtual void render(sf::RenderWindow& ventana) {
        if (!visible) return;

        sf::CircleShape cuerpo(radius);
        cuerpo.setOrigin({ radius, radius });
        cuerpo.setPosition(position);
        cuerpo.setFillColor(color);
        cuerpo.setOutlineThickness(2.f);
        cuerpo.setOutlineColor(sf::Color::Black);
        ventana.draw(cuerpo);
    }

    sf::Vector2f getPosition() const {
        return position;
    }
};

class Robot : public Entity {
public:
    Robot(Nodo* nodoInicial, int tileWidth) : Entity(nodoInicial, tileWidth) {
        nombre = "Robot";
        color = sf::Color::Yellow;
    }

    Direccion chooseDirection(const vector<Direccion>& dirs) {
        vector<Direccion> prioridad = {
            Direccion::RIGHT,
            Direccion::DOWN,
            Direccion::LEFT,
            Direccion::UP
        };

        for (Direccion p : prioridad) {
            for (Direccion d : dirs) {
                if (p == d) return d;
            }
        }

        return dirs[0];
    }

    void update(float dt) override {
        position += directions[direction] * speed * dt;

        if (overshotTarget()) {
            node = target;

            vector<Direccion> dirs = validDirections();
            Direccion dir = chooseDirection(dirs);

            target = getNewTarget(dir);
            if (target != node) {
                direction = dir;
            }
            else {
                target = getNewTarget(direction);
            }

            setPosition();
        }
    }
};

class Ghost : public Entity {
public:
    Ghost(Nodo* nodoInicial, int tileWidth) : Entity(nodoInicial, tileWidth) {
        nombre = "Ghost";
        color = sf::Color(255, 105, 180);
    }
};

int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    try {
        sf::RenderWindow ventana(sf::VideoMode({ 1100u, 700u }), "Robot Aspirador");
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

        sf::RectangleShape mueble({ 90.f * escala, 260.f * escala });
        mueble.setPosition({ offsetX + 101.f * escala, offsetY + 150.f * escala });
        mueble.setFillColor(sf::Color(120, 80, 40));
        mueble.setOutlineThickness(2.f);
        mueble.setOutlineColor(sf::Color::Black);

        vector<Zona> zonas(4);

        zonas[0].nombre = "Zona 1";
        zonas[0].largoCm = 500;
        zonas[0].anchoCm = 150;
        zonas[0].forma.setSize({ 500.f * escala, 150.f * escala });
        zonas[0].forma.setPosition({ offsetX, offsetY });
        zonas[0].forma.setFillColor(sf::Color(255, 200, 200, 180));
        zonas[0].forma.setOutlineThickness(2.f);
        zonas[0].forma.setOutlineColor(sf::Color::Red);

        zonas[1].nombre = "Zona 2";
        zonas[1].largoCm = 480;
        zonas[1].anchoCm = 101;
        zonas[1].forma.setSize({ 101.f * escala, 480.f * escala });
        zonas[1].forma.setPosition({ offsetX, offsetY + 150.f * escala });
        zonas[1].forma.setFillColor(sf::Color(200, 255, 200, 180));
        zonas[1].forma.setOutlineThickness(2.f);
        zonas[1].forma.setOutlineColor(sf::Color::Green);

        zonas[2].nombre = "Zona 3";
        zonas[2].largoCm = 309;
        zonas[2].anchoCm = 480;
        zonas[2].forma.setSize({ 309.f * escala, 480.f * escala });
        zonas[2].forma.setPosition({ offsetX + 191.f * escala, offsetY + 150.f * escala });
        zonas[2].forma.setFillColor(sf::Color(200, 200, 255, 180));
        zonas[2].forma.setOutlineThickness(2.f);
        zonas[2].forma.setOutlineColor(sf::Color::Blue);

        zonas[3].nombre = "Zona 4";
        zonas[3].largoCm = 90;
        zonas[3].anchoCm = 220;
        zonas[3].forma.setSize({ 90.f * escala, 220.f * escala });
        zonas[3].forma.setPosition({ offsetX + 101.f * escala, offsetY + 410.f * escala });
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
        info << "Nivel 3:\n\n";
        info << "Robot = entidad\n\n";
        info << "Ghost = enemigo\n\n";
        info << "Ghost se mueve solo\n\n";
        info << "Controles:\n\n";
        info << "ESC -> salir";

        sf::Text textoInfo(fuente, info.str(), 12);
        textoInfo.setFillColor(sf::Color::Black);
        textoInfo.setPosition({ 650.f, 90.f });

        // Laberinto del nivel 2
        NodeGroup nodes("mazetest.txt", 45, 45,
            static_cast<int>(offsetX + 30),
            static_cast<int>(offsetY + 30));

        Nodo* nodoInicioRobot = nodes.getStartTempNode();
        if (nodoInicioRobot == nullptr) {
            throw runtime_error("No hay nodos en el laberinto.");
        }

        Nodo* nodoInicioGhost = nodes.getNthNode(2);
        if (nodoInicioGhost == nullptr) {
            nodoInicioGhost = nodoInicioRobot;
        }

        Robot robot(nodoInicioRobot, 45);
        Ghost ghost(nodoInicioGhost, 45);

        while (ventana.isOpen()) {
            float dt = 1.f / 60.f;

            while (auto evento = ventana.pollEvent()) {
                if (evento->is<sf::Event::Closed>()) {
                    ventana.close();
                }
            }

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape)) {
                ventana.close();
            }

            robot.update(dt);
            ghost.update(dt);

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

            robot.render(ventana);
            ghost.render(ventana);

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
