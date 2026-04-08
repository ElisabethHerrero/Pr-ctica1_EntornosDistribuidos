#ifndef VISUALIZER_H
#define VISUALIZER_H

#include "repository.h"
#include "event_bus.h"

#include <SFML/Graphics.hpp>
#include <iostream>
#include <sstream>

class Visualizer
{
private:
    Repository& repository;
    EventBus& eventBus;

    sf::RenderWindow window;
    sf::Font font;
    std::string lastEventText = "Esperando eventos...";

public:
    Visualizer(Repository& repo, EventBus& bus)
        : repository(repo), eventBus(bus),
        window(sf::VideoMode(1000, 800), "Simulador Robot Aspirador - SFML")
    {
        if (!font.loadFromFile("dogica.ttf"))
        {
            std::cerr << "No se pudo cargar la fuente arial.ttf\n";
        }
    }

    sf::Color getZoneColor(ZoneStatus status)
    {
        switch (status)
        {
        case ZoneStatus::PENDING:  return sf::Color(220, 220, 220);
        case ZoneStatus::ASSIGNED: return sf::Color(255, 220, 100);
        case ZoneStatus::CLEANING: return sf::Color(100, 180, 255);
        case ZoneStatus::FINISHED: return sf::Color(120, 220, 120);
        default: return sf::Color::White;
        }
    }

    void drawRoom()
    {
        // Marco exterior habitación
        sf::RectangleShape roomBorder(sf::Vector2f(560, 690));
        roomBorder.setPosition(150, 50);
        roomBorder.setFillColor(sf::Color(70, 70, 70));
        window.draw(roomBorder);

        // Suelo interior
        sf::RectangleShape roomInside(sf::Vector2f(500, 630));
        roomInside.setPosition(180, 80);
        roomInside.setFillColor(sf::Color(245, 245, 245));
        window.draw(roomInside);

        // Mueble central
        sf::RectangleShape furniture(sf::Vector2f(100, 260));
        furniture.setPosition(280, 230);
        furniture.setFillColor(sf::Color(150, 100, 60));
        window.draw(furniture);
    }

    void drawZones()
    {
        for (const auto& zone : repository.getZones())
        {
            sf::RectangleShape rect(sf::Vector2f(zone.width, zone.height));
            rect.setPosition(zone.x, zone.y);
            rect.setFillColor(getZoneColor(zone.status));
            rect.setOutlineColor(sf::Color(255, 140, 0));
            rect.setOutlineThickness(2.0f);
            window.draw(rect);

            sf::Text label;
            label.setFont(font);
            label.setString(zone.name);
            label.setCharacterSize(20);
            label.setFillColor(sf::Color::Black);
            label.setPosition(zone.x + 10, zone.y + 10);
            window.draw(label);
        }
    }

    void drawInfo()
    {
        float totalArea = repository.calculateTotalArea();

        sf::Text title;
        title.setFont(font);
        title.setString("Simulacion distribuida de limpieza");
        title.setCharacterSize(28);
        title.setFillColor(sf::Color::Black);
        title.setPosition(50, 10);
        window.draw(title);

        sf::Text info;
        info.setFont(font);
        info.setCharacterSize(18);
        info.setFillColor(sf::Color::Black);

        std::ostringstream oss;
        oss << "Area total: " << totalArea << " cm2\n";
        oss << "Zona 1 = 500 x 150 = 75000 cm2\n";
        oss << "Zona 2 = 480 x 101 = 48480 cm2\n";
        oss << "Zona 3 = 309 x 480 = 148320 cm2\n";
        oss << "Zona 4 = 90 x 220 = 19800 cm2\n";

        info.setString(oss.str());
        info.setPosition(730, 120);
        window.draw(info);

        sf::Text eventText;
        eventText.setFont(font);
        eventText.setCharacterSize(18);
        eventText.setFillColor(sf::Color::Red);
        eventText.setString("Ultimo evento: " + lastEventText);
        eventText.setPosition(50, 760);
        window.draw(eventText);
    }

    void processEvents()
    {
        while (window.isOpen())
        {
            sf::Event ev;
            while (window.pollEvent(ev))
            {
                if (ev.type == sf::Event::Closed)
                    window.close();
            }

            auto event = eventBus.poll();
            if (event.has_value())
            {
                lastEventText = event->message;
                std::cout << "[EVENTO] " << event->message << "\n";
            }

            window.clear(sf::Color::White);
            drawRoom();
            drawZones();
            drawInfo();
            window.display();
        }
    }
};

#endif
