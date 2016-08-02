#include "stdafx.h"

#include "animation.h"

Animation::Animation(double d) : duration(d) {}

bool Animation::isDone(double time) const {
  return time > begin + duration;
}

void Animation::setBegin(double time) {
  CHECK(begin == -1);
  begin = time;
}

void Animation::render(Renderer& r, Rectangle bounds, Vec2 origin, double time) {
  CHECK(begin >= 0) << begin;
  CHECK(time - begin <= duration) << time << " " << begin << " " << duration;
  renderSpec(r, bounds, origin, (time - begin) / duration);
}

class ThrownObject : public Animation {
  public:
  ThrownObject(Vec2 dir, ViewObject obj, bool sprite, Vec2 sz)
    : Animation(200), direction(dir), viewObject(obj), useSprite(sprite), squareSize(sz) {}

  virtual void renderSpec(Renderer& renderer, Rectangle bounds, Vec2 origin, double state) {
    int x = origin.x + state * direction.x;
    int y = origin.y + state * direction.y;
    renderer.drawViewObject(Vec2(x, y), viewObject, useSprite, squareSize);
  }

  private:
  Vec2 direction;
  ViewObject viewObject;
  bool useSprite;
  Vec2 squareSize;
};

PAnimation Animation::thrownObject(Vec2 direction, ViewObject obj, bool useSprite, Vec2 squareSize) {
  return PAnimation(new ThrownObject(direction, obj, useSprite, squareSize));
}

class SpriteAnim : public Animation {
  public:
  struct FrameInfo {
    Vec2 origin;
    Vec2 size;
    Vec2 offset;
  };
  SpriteAnim(double frame, int tile, vector<FrameInfo> f)
      : Animation(frame * f.size()), frames(f), tileNum(tile) {}

  virtual void renderSpec(Renderer& renderer, Rectangle bounds, Vec2 origin, double state) {
    return;
    FrameInfo current = frames[min<int>(frames.size() - 1, max(0, int(state * frames.size())))];
    renderer.drawSprite(origin + current.offset, current.origin, current.size, renderer.tiles[tileNum]);
  }

  private:
  vector<FrameInfo> frames;
  int tileNum;
};
 
PAnimation Animation::fromId(AnimationId id) {
  return PAnimation(new SpriteAnim(50, 6, {
        {{510, 628}, {36, 36}, {0, 0}},
        {{683, 611}, {70, 70}, {-17, -17}},
        {{577, 598}, {94, 94}, {-29, -29}},
        }));
}
