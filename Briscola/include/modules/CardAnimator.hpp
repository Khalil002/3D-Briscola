#pragma once
#include <vector>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

class CardAnimator {
public:
  CardAnimator(std::function<void(int, const glm::mat4&)> setWm,
               std::function<glm::mat4(int)> getWm)
  : setMatrix(std::move(setWm)), getMatrix(std::move(getWm)) {}

  // API: enqueue per-card steps (executed sequentially per id)
  void addMove(int idx, const glm::mat4& curWm,
               glm::vec3 toPos, float seconds) {
    Track& t = getOrCreateTrack(idx, curWm);
    Step s; s.type = Step::MOVE; s.move.to = toPos; s.move.dur = dur(seconds);
    t.queue.push_back(s);
  }

  // angleDeg in degrees. localAxis=true → rotate around card’s local axis.
  void addRotate(int idx, const glm::mat4& curWm,
                 float angleDeg, glm::vec3 axis, float seconds, bool localAxis) {
    Track& t = getOrCreateTrack(idx, curWm);
    Step s; s.type = Step::ROTATE;
    s.rot.delta = glm::angleAxis(glm::radians(angleDeg), glm::normalize(axis));
    s.rot.dur   = dur(seconds);
    s.rot.local = localAxis;
    t.queue.push_back(s);
  }

  void addMoveAndRotate(int idx, const glm::mat4& curWm,
                        glm::vec3 toPos, float moveSec,
                        float angleDeg, glm::vec3 axis, float rotSec,
                        bool localAxis) {
    Track& t = getOrCreateTrack(idx, curWm);
    Step s; s.type = Step::MOVE_ROTATE;
    s.move.to     = toPos; s.move.dur = dur(moveSec);
    s.rot.delta   = glm::angleAxis(glm::radians(angleDeg), glm::normalize(axis));
    s.rot.dur     = dur(rotSec);
    s.rot.local   = localAxis;
    t.queue.push_back(s);
  }

  void addWait(int idx, const glm::mat4& curWm, float seconds) {
    Track& t = getOrCreateTrack(idx, curWm);
    Step s; s.type = Step::WAIT; s.wait.dur = dur(seconds);
    t.queue.push_back(s);
  }

  void addGlobalWait(float seconds) {
    float duration = dur(seconds);
    for (auto& kv : tracks) {
      Track& t = kv.second;
      // ensure the track is initialized so wait step makes sense
      glm::mat4 cur = getMatrix(t.idx);
      Step s; s.type = Step::WAIT; s.wait.dur = duration;
      t.queue.push_back(s);
    }
  }

  // Advance all tracks
  void tick(float dt) {
    for (auto& kv : tracks) {
      Track& t = kv.second;
      if (!t.active || t.cur >= t.queue.size()) continue;

      Step& st = t.queue[t.cur];

      if (!t.stepStarted) { // freeze base pose once at step start
        TRS base = decompose(getMatrix(t.idx));
        t.baseP = base.p; t.baseQ = base.q; t.baseS = base.s;
        if (st.type == Step::ROTATE || st.type == Step::MOVE_ROTATE) {
          // Expand target q1 from frozen base
          glm::quat target = st.rot.local ?
              glm::normalize(t.baseQ * st.rot.delta) :
              glm::normalize(st.rot.delta * t.baseQ);
          st.rot.target = target;
        }
        t.elapsed = 0.f;
        t.stepStarted = true;
      }

      t.elapsed += dt;
      glm::vec3 p = t.baseP;
      glm::quat q = t.baseQ;
      glm::vec3 s = t.baseS;

      switch (st.type) {
        case Step::WAIT: {
          if (t.elapsed >= st.wait.dur) advance(t, p, q, s);
        } break;

        case Step::MOVE: {
          float u = ease01(t.elapsed / st.move.dur);
          p = glm::mix(t.baseP, st.move.to, u);
          write(t.idx, p, q, s);
          if (t.elapsed >= st.move.dur) advance(t, p, q, s);
        } break;

        case Step::ROTATE: {
          float u = ease01(t.elapsed / st.rot.dur);
          q = glm::slerp(t.baseQ, st.rot.target, u);
          write(t.idx, p, q, s);
          if (t.elapsed >= st.rot.dur) advance(t, p, q, s);
        } break;

        case Step::MOVE_ROTATE: {
          float mu = ease01(t.elapsed / st.move.dur);
          float ru = ease01(t.elapsed / st.rot.dur);
          p = glm::mix(t.baseP, st.move.to, mu);
          q = glm::slerp(t.baseQ, st.rot.target, ru);
          write(t.idx, p, q, s);
          if (t.elapsed >= std::max(st.move.dur, st.rot.dur)) advance(t, p, q, s);
        } break;
      }
    }
  }

private:
  static inline float dur(float sec) { return sec <= 0.f ? 1e-4f : sec; }
  static inline float clamp01(float x){ return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
  static inline float ease01(float t) { t = clamp01(t); return t*t*(3.f - 2.f*t); }

  struct TRS { glm::vec3 p{0}; glm::quat q{1,0,0,0}; glm::vec3 s{1}; };

  static TRS decompose(const glm::mat4& M) {
    TRS r;
    r.p = glm::vec3(M[3]);
    glm::vec3 X(M[0]), Y(M[1]), Z(M[2]);
    r.s = { glm::length(X), glm::length(Y), glm::length(Z) };
    glm::mat3 R(
      glm::vec3(M[0]) / (r.s.x == 0 ? 1.f : r.s.x),
      glm::vec3(M[1]) / (r.s.y == 0 ? 1.f : r.s.y),
      glm::vec3(M[2]) / (r.s.z == 0 ? 1.f : r.s.z)
    );
    r.q = glm::normalize(glm::quat_cast(R));
    return r;
  }

  struct Step {
    enum Type { MOVE, ROTATE, MOVE_ROTATE, WAIT } type{};
    struct { glm::vec3 to{0}; float dur{0}; } move;
    struct { glm::quat delta{1,0,0,0}; glm::quat target{1,0,0,0}; float dur{0}; bool local{true}; } rot;
    struct { float dur{0}; } wait;
  };

  struct Track {
    int idx{-1};
    std::vector<Step> queue;
    size_t cur{0};
    float elapsed{0};
    bool active{false};
    bool stepStarted{false};
    glm::vec3 baseP{0}; glm::quat baseQ{1,0,0,0}; glm::vec3 baseS{1};
  };

  Track& getOrCreateTrack(int idx, const glm::mat4& curWm) {
    auto it = tracks.find(idx);
    if (it == tracks.end()) {
      Track t; t.idx = idx; t.active = true; t.stepStarted = false;
      TRS base = decompose(curWm);
      t.baseP = base.p; t.baseQ = base.q; t.baseS = base.s;
      it = tracks.emplace(idx, std::move(t)).first;
    } else {
      it->second.active = true;
    }
    return it->second;
  }

  void advance(Track& t, const glm::vec3& p, const glm::quat& q, const glm::vec3& s) {
    // carry the final pose of the finished step as the new base
    write(t.idx, p, q, s);
    t.baseP = p; t.baseQ = q; t.baseS = s;
    t.cur++; t.elapsed = 0.f; t.stepStarted = false;
    if (t.cur >= t.queue.size()) t.active = false;
  }

  void write(int idx, const glm::vec3& p, const glm::quat& q, const glm::vec3& s) {
    glm::mat4 T = glm::translate(glm::mat4(1), p);
    glm::mat4 R = glm::mat4_cast(glm::normalize(q));
    glm::mat4 S = glm::scale(glm::mat4(1), s);
    setMatrix(idx, T * R * S);
  }

public:
  std::function<void(int, const glm::mat4&)> setMatrix;
  std::function<glm::mat4(int)>              getMatrix;
  std::unordered_map<int, Track>             tracks;
};
