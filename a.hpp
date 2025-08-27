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

    // --- Enqueue steps -------------------------------------------------------

    void addMove(int idx, const glm::mat4& curWm, glm::vec3 toPos, float seconds) {
        Track& t = getOrCreateTrack(idx, curWm);
        Step s; s.type = Step::MOVE;
        s.mov.to = toPos;
        s.duration = dur(seconds);
        t.queue.push_back(s);
    }

    void addRotate(int idx, const glm::mat4& curWm, float degrees, glm::vec3 axis,
                   float seconds, bool localAxis = true) {
        Track& t = getOrCreateTrack(idx, curWm);
        Step s; s.type = Step::ROTATE;
        s.rot.deg = degrees;
        s.rot.axis = glm::normalize(axis);
        s.rot.local = localAxis;
        s.duration = dur(seconds);
        t.queue.push_back(s);
    }

    void addWait(int idx, const glm::mat4& curWm, float seconds) {
        Track& t = getOrCreateTrack(idx, curWm);
        Step s; s.type = Step::WAIT;
        s.duration = dur(seconds);
        t.queue.push_back(s);
    }

    // 🔹 NEW: move and rotate at the same time
    void addMoveAndRotate(int idx, const glm::mat4& curWm,
                          glm::vec3 toPos, float moveSec,
                          float degrees, glm::vec3 axis, float rotSec,
                          bool localAxis = true) {
        Track& t = getOrCreateTrack(idx, curWm);
        Step s; s.type = Step::MOVE_ROTATE;
        s.mov.to = toPos;
        s.duration = dur(std::max(moveSec, rotSec)); // both run over the longer duration
        s.rot.deg = degrees;
        s.rot.axis = glm::normalize(axis);
        s.rot.local = localAxis;
        s.rot.rotDur = dur(rotSec);
        s.mov.movDur = dur(moveSec);
        t.queue.push_back(s);
    }

    // --- Tick all tracks -----------------------------------------------------
    void tick(float dt) {
        for (auto it = tracks.begin(); it != tracks.end(); ) {
            Track& tr = it->second;
            if (tr.queue.empty()) { it = tracks.erase(it); continue; }

            if (!tr.active) startNextStep(tr);

            Step& st = tr.queue.front();
            tr.elapsed += dt;

            float u = clamp01(tr.elapsed / st.duration);
            float e = ease(u);

            glm::vec3 p = tr.baseP;
            glm::quat q = tr.baseQ;
            const glm::vec3 s = tr.baseS;

            switch (st.type) {
                case Step::MOVE: {
                    p = glm::mix(tr.baseP, st.mov.to, e);
                } break;

                case Step::ROTATE: {
                    q = glm::slerp(tr.baseQ, st.rot.q1, e);
                } break;

                case Step::WAIT: {
                    // hold
                } break;

                // 🔹 Combined
                case Step::MOVE_ROTATE: {
                    float mu = ease(clamp01(tr.elapsed / st.mov.movDur));
                    float ru = ease(clamp01(tr.elapsed / st.rot.rotDur));
                    p = glm::mix(tr.baseP, st.mov.to, mu);
                    q = glm::slerp(tr.baseQ, st.rot.q1, ru);
                } break;
            }

            writeWm(tr.idx, p, q, s);

            if (tr.elapsed >= st.duration) {
                // Commit endpoint
                switch (st.type) {
                    case Step::MOVE:
                        tr.baseP = st.mov.to;
                        break;
                    case Step::ROTATE:
                        tr.baseQ = st.rot.q1;
                        break;
                    case Step::MOVE_ROTATE:
                        tr.baseP = st.mov.to;
                        tr.baseQ = st.rot.q1;
                        break;
                    case Step::WAIT: break;
                }
                tr.queue.erase(tr.queue.begin());
                tr.active = false;
                tr.elapsed = 0.f;
                writeWm(tr.idx, tr.baseP, tr.baseQ, tr.baseS);
            }

            ++it;
        }
    }

    void clear(int idx) { tracks.erase(idx); }
    void clearAll()     { tracks.clear(); }

private:
    struct Step {
        enum Type { MOVE, ROTATE, WAIT, MOVE_ROTATE } type;
        float duration = 0.0001f;

        struct { glm::vec3 to{0}; float movDur=0.f; } mov;
        struct { float deg=0.f; glm::vec3 axis{0,1,0}; bool local=true;
                 glm::quat q1{1,0,0,0}; float rotDur=0.f; } rot;
    };

    struct Track {
        int idx = -1;
        glm::vec3 baseP{0,0,0};
        glm::quat baseQ{1,0,0,0};
        glm::vec3 baseS{1,1,1};
        bool  active   = false;
        float elapsed  = 0.f;
        std::vector<Step> queue;
    };

    static void decomposeTRS(const glm::mat4& M, glm::vec3& T, glm::quat& R, glm::vec3& S) {
        glm::vec3 c0 = glm::vec3(M[0]), c1 = glm::vec3(M[1]), c2 = glm::vec3(M[2]);
        S = glm::vec3(glm::length(c0), glm::length(c1), glm::length(c2));
        glm::vec3 invS = { S.x?1.f/S.x:0.f, S.y?1.f/S.y:0.f, S.z?1.f/S.z:0.f };
        glm::mat3 rotM(c0*invS.x, c1*invS.y, c2*invS.z);
        R = glm::quat_cast(rotM);
        T = glm::vec3(M[3]);
    }

    static float ease(float t){ return t*t*(3.0f - 2.0f*t); }
    static float clamp01(float x){ return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
    static float dur(float s){ return s <= 0.f ? 0.0001f : s; }

    Track& getOrCreateTrack(int idx, const glm::mat4& curWm) {
        auto it = tracks.find(idx);
        if (it != tracks.end()) return it->second;
        Track tr; tr.idx = idx;
        decomposeTRS(curWm, tr.baseP, tr.baseQ, tr.baseS);
        return tracks.emplace(idx, std::move(tr)).first->second;
    }

    void startNextStep(Track& tr) {
        if (tr.queue.empty()) return;
        Step& st = tr.queue.front();
        tr.elapsed = 0.f;
        tr.active  = true;

        if (st.type == Step::ROTATE || st.type == Step::MOVE_ROTATE) {
            glm::quat dq = glm::angleAxis(glm::radians(st.rot.deg), st.rot.axis);
            st.rot.q1 = st.rot.local ? (tr.baseQ * dq) : (dq * tr.baseQ);
        }
    }

    void writeWm(int idx, const glm::vec3& p, const glm::quat& q, const glm::vec3& s) {
        glm::mat4 T = glm::translate(glm::mat4(1), p);
        glm::mat4 R = glm::mat4_cast(q);
        glm::mat4 S = glm::scale(glm::mat4(1), s);
        setMatrix(idx, T * R * S);
    }

    std::function<void(int, const glm::mat4&)> setMatrix;
    std::function<glm::mat4(int)>              getMatrix;
    std::unordered_map<int, Track> tracks;
};
