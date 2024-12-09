#include "Scene.hpp"
#include "../Kernel/Kernel.hpp"
#include "../../Components/Camera/Camera.hpp"
#include "../../Components/Rigidbody2d/Rigidbody2d.hpp"
#include "../../Components/ParticleSystem/ParticleSystem.hpp"
#include "../../Components/SpriteRenderer/SpriteRenderer.hpp"
#include "../../Components/Animation2d/Animation2d.hpp"
#include "../../UI/TextInput/TextInput.hpp"
#include "../Json/Json.hpp"
#include <thread>

using namespace alce;

Scene::Scene(String name)
{
    this->name = name;
}

void Scene::InitPhysics(Vector2 gravity)
{
    world = std::make_shared<B2World>(gravity);
}

void Scene::Save()
{
    UpdateJson();
    std::thread task([&](){
        json.SaveAsFile(GetName(), "./Scenes/");
    });
    task.join();
}

void Scene::UpdateJson()
{
    json.FromString("{}");

    for(auto& sl : sortingLayers)
    {
        for(auto& go : *sl.second.get())
        {
            String jsonStr = R"({
                "id": "$id", 
                "alias": "$alias", 
                "transform": {
                    "position": "$position",
                    "rotation": $rotation,
                    "scale": "$scale"
                }
            })";

            jsonStr.Replace("$id", go->id);

            if(go->alias != false)
            {
                jsonStr.Replace("$alias", go->alias);
            }

            jsonStr.Replace("$position", go->transform.position.ToString());
            jsonStr.Replace("$rotation", go->transform.rotation);
            jsonStr.Replace("$scale", go->transform.scale.ToString());

            Json obj(jsonStr);

            json.Set(go->alias == false ? go->id : go->alias, obj);
            
        }
    }
}

void Scene::AddGameObject(GameObjectPtr gameObject, String alias)
{
    try
    {
        if(!sortingLayers.GetKeyList().Contains(gameObject->sortingLayer))
        {
            GameObjectListPtr list = std::make_shared<List<GameObjectPtr>>();
            list.get()->Add(gameObject);

            sortingLayers.Set(gameObject->sortingLayer, list);
            gameObject->scene = this;

            gameObject->alias = alias;

            if(persist)
            {
                UpdateJson();
            }
            return;
        }

        if(sortingLayers[gameObject->sortingLayer].get()->Contains(gameObject))
        {
            Debug.Warning("Scene already contains gameObject \"{}\"", {gameObject->id});
            return;
        }

        sortingLayers[gameObject->sortingLayer].get()->Add(gameObject);
        gameObject->scene = this;

        gameObject->alias = alias;

        if(persist)
        {
            UpdateJson();
            if(!JsonFileExists()) 
                Save();
        }
    }
    catch(const std::exception& e)
    {
        Debug.Warning("Internal error: {}", {std::string(e.what())});
    }
}

List<GameObjectPtr> Scene::GetAllGameObjects()
{
    List<GameObjectPtr> gameObjects;

    for(auto& i: sortingLayers.GetValueList())
    {
        gameObjects.Merge(*i.get());
    }

    return gameObjects;
}

void Scene::AddCanvas(CanvasPtr canvas, ComponentPtr camera)
{
    if(canvasList.Contains(canvas))
    {
        Debug.Warning("Scene already contains Canvas \"{}\"", {canvas->id});
        return;
    }

    canvasList.Add(canvas);
    canvas->view = &((Camera*)camera.get())->view;
    canvas->rotation = &camera->transform->rotation;
    canvas->scale = &((Camera*)camera.get())->zoom;
}

String Scene::GetName()
{
    return name;
}

void Scene::Pause(bool flag)
{
    paused = flag;
}

bool Scene::IsPaused()
{
    return paused;
}

void Scene::Clear()
{
    sortingLayers.Clear();
}

B2WorldPtr Scene::GetWorld()
{
    return world;
}

void Scene::DevelopmentMode(bool flag)
{
    developmentMode = flag;
}

void Scene::Shell(String command)
{
    //TODO:
}

void Scene::EventsManager(sf::Event& e)
{
    switch(e.type)
    {
    case sf::Event::LostFocus:
        if(Alce.stanby)
            paused = true;
        break;
    case sf::Event::GainedFocus:
        if(paused) paused = false;
        break;
    case sf::Event::Closed:
        Alce.GetWindow().close();
        break;
    }

    for(auto& sortingLayer: sortingLayers)
    {
        for(auto& gameObject: *sortingLayer.second.get())
        {
            gameObject->EventManager(e);

            for(auto& component: gameObject->GetComponents())
            {
                component->EventManager(e);
            }
        }
    }
}

void Scene::Render()
{
    for(auto& _camera: cameras)
    {
        Camera* camera = (Camera*) _camera;

        if(!camera->enabled) continue;

        Alce.GetWindow().setView(camera->view);

        auto layers = sortingLayers.GetKeyList();
        int max = Math.Max<int>(~layers);

        for(int i = max; i >= 0; i--)
        {
            if(!layers.Contains(i)) continue;
            
            for(auto& gameObject: *sortingLayers.Get(i).get())
            {
                if(!gameObject->enabled) continue;

                if(gameObject->cardinals.Empty())
                {
                    if(!camera->GetBounds().InArea(gameObject->transform.position.ToPixels())) continue;
                }
                else
                {
                    if(!camera->GetBounds().InArea(*gameObject->cardinals["top-left"].get()) &&
                    !camera->GetBounds().InArea(*gameObject->cardinals["top-right"].get()) &&
                    !camera->GetBounds().InArea(*gameObject->cardinals["bottom-left"].get()) &&
                    !camera->GetBounds().InArea(*gameObject->cardinals["bottom-right"].get()))
                    {
                        continue;
                    }
                }

                gameObject->Render();
            }

            if(developmentMode)
            {
                for(auto& gameObject: *sortingLayers.Get(i).get())
                {
                    if(!gameObject->enabled) continue;

                    if(gameObject->cardinals.Empty())
                    {
                        if(!camera->GetBounds().InArea(gameObject->transform.position.ToPixels())) continue;
                    }
                    else
                    {
                        if(!camera->GetBounds().InArea(*gameObject->cardinals["top-left"].get()) &&
                        !camera->GetBounds().InArea(*gameObject->cardinals["top-right"].get()) &&
                        !camera->GetBounds().InArea(*gameObject->cardinals["bottom-left"].get()) &&
                        !camera->GetBounds().InArea(*gameObject->cardinals["bottom-right"].get()))
                        {
                            continue;
                        }
                    }

                    for(auto& component: gameObject->GetComponents())
                    {
                        component->DebugRender();
                    }

                    gameObject->DebugRender();
                }         
            }
        }    
    }

    if(developmentMode)
    {
        for(auto& _camera: cameras)
        {
            Camera* camera = (Camera*) _camera;
            RenderGrid(Alce.GetWindow(), camera->view);
        }
    }

    for(auto& canvas: canvasList)
    {
        if(canvas->enabled)
        {
            canvas->Render();
        }
    }
}

void Scene::Update()
{
    if(paused) return;

    if(world != nullptr) world->Step();

    for(auto& sortingLayer: sortingLayers)
    {
        for(auto& gameObject: *sortingLayer.second.get())
        {
            if(!gameObject->enabled) continue;
            
            gameObject->Update();

            for(auto& component: gameObject->GetComponents())
            {
                if(!component->enabled) continue;
                component->Update();

                if(component->id == "SpriteRenderer")
                    SetCardinals(gameObject,  gameObject->GetComponent<SpriteRenderer>()->GetCardinals());

                if(component->id == "Animation2d")
                    SetCardinals(gameObject, gameObject->GetComponent<Animation2d>()->GetCardinals());
            }
        }

        if(sortingLayer.second != nullptr)
        {
            sortingLayer.second->RemoveIf([](GameObjectPtr gameObject){
                return gameObject->destroyed;
            });
        }
    }

    for(auto& canvas: canvasList)
    {
        if(canvas->enabled)
        {
            canvas->Update();
        }
    }

}

void Scene::SetCardinals(GameObjectPtr gameObject, Dictionary<String, Vector2Ptr> cardinals)
{
    if(!gameObject->cardinals.HasKey("top-left") || cardinals.HasKey("top-left") && (
    gameObject->cardinals["top-left"]->y > cardinals["top-left"]->y ||
    gameObject->cardinals["top-left"]->x > cardinals["top-left"]->x))
    {
        gameObject->cardinals.Set("top-left", cardinals["top-left"]);
    }

    if(!gameObject->cardinals.HasKey("top-right") || cardinals.HasKey("top-right") && ( 
    gameObject->cardinals["top-right"]->y > cardinals["top-right"]->y ||
    gameObject->cardinals["top-right"]->x < cardinals["top-right"]->x))
    {
        gameObject->cardinals.Set("top-right", cardinals["top-right"]);
    }

    if(!gameObject->cardinals.HasKey("bottom-left") || cardinals.HasKey("bottom-left") && (
    gameObject->cardinals["bottom-left"]->y < cardinals["bottom-left"]->y ||
    gameObject->cardinals["bottom-left"]->x > cardinals["bottom-left"]->x))
    {
        gameObject->cardinals.Set("bottom-left", cardinals["bottom-left"]);
    }

    if(!gameObject->cardinals.HasKey("bottom-right") || cardinals.HasKey("bottom-right") && (
    gameObject->cardinals["bottom-right"]->y < cardinals["bottom-right"]->y ||
    gameObject->cardinals["bottom-right"]->x < cardinals["bottom-right"]->x))
    {
        gameObject->cardinals.Set("bottom-right", cardinals["bottom-right"]);
    }
}

void Scene::RenderGrid(sf::RenderWindow& window, const sf::View& view)
{
    sf::Vector2f topLeft = window.mapPixelToCoords(sf::Vector2i(0, 0), view);
    sf::Vector2f bottomRight = window.mapPixelToCoords(sf::Vector2i(window.getSize().x, window.getSize().y), view);

    float worldStartX = topLeft.x / PPM;
    float worldEndX = bottomRight.x / PPM;
    float worldStartY = topLeft.y / PPM;
    float worldEndY = bottomRight.y / PPM;

    float startX = std::floor(worldStartX / GridScale) * GridScale;
    float endX = std::ceil(worldEndX / GridScale) * GridScale;
    float startY = std::floor(worldStartY / GridScale) * GridScale;
    float endY = std::ceil(worldEndY / GridScale) * GridScale;

    std::vector<sf::Vertex> lines;

    float axisThickness = 2.0f; 

    for (float x = startX; x <= endX; x += GridScale) 
    {
        float px = x * PPM; 

        if (x == 0) 
        {
            sf::RectangleShape yAxis(sf::Vector2f(axisThickness, (endY - startY) * PPM));
            yAxis.setPosition(px - axisThickness / 2.0f, startY * PPM);
            yAxis.setFillColor(AxisYColor.ToSFMLColor());
            window.draw(yAxis);
        } 
        else 
        {
            sf::Vertex line[] = {
                sf::Vertex(sf::Vector2f(px, startY * PPM), GridColor.ToSFMLColor()),
                sf::Vertex(sf::Vector2f(px, endY * PPM), GridColor.ToSFMLColor())
            };
            window.draw(line, 2, sf::Lines);
        }
    }

    for (float y = startY; y <= endY; y += GridScale) 
    {
        float py = y * PPM;

        if (y == (7 * GridScale))
        {
            sf::RectangleShape xAxis(sf::Vector2f((endX - startX) * PPM, axisThickness));
            xAxis.setPosition(startX * PPM, py - axisThickness / 2.0f);
            xAxis.setFillColor(AxisXColor.ToSFMLColor());
            window.draw(xAxis);
        } 
        else 
        {
            sf::Vertex line[] = {
                sf::Vertex(sf::Vector2f(startX * PPM, py), GridColor.ToSFMLColor()),
                sf::Vertex(sf::Vector2f(endX * PPM, py), GridColor.ToSFMLColor())
            };
            window.draw(line, 2, sf::Lines);
        }
    }

    sf::Font font;
    if (!font.loadFromFile("Assets/fonts/Consolas/CONSOLA.ttf"))
    {
        return;
    }

    for (float x = startX; x <= endX; x += GridScale)
    {
        for (float y = startY; y <= endY; y += GridScale)
        {
            sf::Text text;
            text.setFont(font);
            float displayY = -(y - (7 * GridScale));

            text.setString("(" + std::to_string(static_cast<int>(x)) + ", " + std::to_string(static_cast<int>(displayY)) + ")");
            text.setCharacterSize(GridTextSize);
            text.setFillColor(sf::Color::White);

            float px = x * PPM;
            float py = y * PPM;
            text.setPosition(px + 2, py + 2); 

            window.draw(text);
        }
    }
}
