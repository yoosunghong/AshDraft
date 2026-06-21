# 기술 기획 문서

## 0. 문서 목적

본 문서는 대규모 전장 액션 전략 게임 **AshDraft**의 1차 PoC 제작 범위와 기술 구조를 정의한다.

1차 PoC의 목적은 완성형 게임 제작이 아니라, 다음 핵심 기술이 실제 Unreal Engine 5 프로젝트에서 동작 가능한지 검증하는 것이다.

- 1명의 플레이어블 장수 캐릭터
- 1개의 전장 맵
- 적 장수 및 병사 부대
- 거점 점령 시스템
- 대규모 병력 AI 최적화
- 계층적 AI 구조
- Mass Entity 기반 병사 처리
- Lyra 스타일 프로젝트 구조
- GAS 기반 전투/스킬 구조
- 향후 강화학습 기반 QA 봇 확장 가능성
- 향후 멀티플레이 확장 가능성

---

# 1. 게임 개요

## 1.1 프로젝트명

**AshDraft**

## 1.2 장르

대규모 전장 액션 전략 게임

무쌍류 액션 + 거점 점령 + 부대 단위 AI 전투

## 1.3 1차 PoC 핵심 경험

플레이어는 하나의 전장 맵에서 장수 캐릭터를 조작한다.

전장에는 아군 병사, 적군 병사, 적 장수, 거점이 존재한다.

플레이어는 직접 전투를 수행하면서 거점을 점령하고, 병사 부대의 흐름에 영향을 준다.

전장은 수백 명 이상의 병사가 존재하는 것처럼 보이지만, 실제 연산은 계층형 AI, AI LOD, Mass Entity, Time Slicing을 통해 최적화된다.

## 1.4 1차 PoC 목표

### 기능 목표

- 플레이어블 장수 1명 구현
- 적 장수 1명 구현
- 아군 병사/적군 병사 부대 구현
- 거점 3개 내외 구현
- 거점 점령/소유권 변경 구현
- 플레이어 주변 전투 구현
- 병사 대 병사 집단 전투 구현
- 장수 주변 8방향 슬롯 기반 근접 전투 구현

### 기술 목표

- Lyra 스타일 모듈 구조 적용
- GAS 기반 능력/피해/상태 시스템 적용
- Mass Entity 기반 병사 처리
- ACharacter와 Mass Entity의 하이브리드 사용
- 계층적 AI 구조 구현
- AI LOD 구현
- Tick Rate Throttling / Time Slicing 구현
- NavMesh + Flow Field 기반 이동 실험
- 향후 RL QA Bot 도입을 위한 인터페이스 설계
- 향후 멀티플레이를 고려한 Gameplay Authority 구조 설계

---

# 2. 1차 PoC 범위

## 2.1 포함 범위

| 항목 | 내용 |
| --- | --- |
| 플레이어 | 플레이어블 장수 1명 |
| 적 장수 | 기본 전투 AI를 가진 적 장수 1명 |
| 병사 | 아군/적군 일반 병사 |
| 맵 | 단일 전장 맵 1개 |
| 거점 | 3개 내외의 점령 가능 거점 |
| 전투 | 기본 공격, 피격, 사망, 넉백 |
| AI | 계층적 AI, AI LOD, Mass Entity 병사 |
| 이동 | NavMesh, Flow Field PoC |
| 구조 | Lyra 스타일 Game Feature 구조 |
| 능력 | GAS 기반 공격/피해 처리 |
| QA 확장 | 향후 플레이어 QA Bot을 위한 입력/상태 인터페이스 |

## 2.2 제외 범위

1차 PoC에서는 다음 항목을 제외한다.

- 완성형 캠페인
- 복수 플레이어블 캐릭터
- 복잡한 장비/성장 시스템
- 온라인 멀티플레이 실제 구현
- 완성형 강화학습 훈련 환경
- 복잡한 외교/전략 시뮬레이션
- 고품질 시네마틱
- 상용 수준의 UI/UX
- 완성형 맵 자동 생성 시스템

단, 제외 항목이라도 향후 확장을 고려한 구조는 미리 설계한다.

---

# 3. 핵심 게임 루프

## 3.1 기본 플레이 루프

```
flowchart TD
    A[전장 시작] --> B[플레이어 장수 조작]
    B --> C[근처 병사 및 적 장수와 전투]
    C --> D[거점으로 이동]
    D --> E[거점 내 적 병력 제거]
    E --> F[거점 점령 게이지 증가]
    F --> G[거점 소유권 변경]
    G --> H[아군 병력 흐름 변경]
    H --> I[적 본진 또는 주요 장수 격파]
```

## 3.2 1차 PoC 승리 조건

PoC에서는 단순한 승리 조건을 사용한다.

- 모든 거점 점령

## 3.3 패배 조건

- 플레이어 사망
- 또는 아군 본거점 점령당함

---

# 4. 맵 설계

## 4.1 맵 구성

1차 PoC 맵은 단일 전장으로 구성한다.

### 기본 구조

- 아군 시작 거점
- 중앙 중립 거점
- 적군 시작 거점
- 주요 이동로 2~3개
- 병목 지형 1개
- 넓은 교전 지역 1개

```
flowchart LR
    A[아군 본거점] --- B[좌측 이동로]
    A --- C[중앙 전장]
    A --- D[우측 이동로]
    B --- E[중앙 거점]
    C --- E
    D --- E
    E --- F[적군 본거점]
```

## 4.2 맵 설계 목적

1차 PoC 맵은 재미보다 기술 검증에 초점을 둔다.

검증해야 할 요소는 다음과 같다.

- 병사들이 거점 간 이동 목표를 공유할 수 있는가?
- Flow Field 기반 이동이 가능한가?
- 병목 지형에서 병사 밀집이 과도하게 발생하지 않는가?
- 플레이어 주변 병사만 정교하게 동작하고, 원거리 병사는 단순화되는가?
- 거점 소유권 변화가 부대 목표 변경으로 연결되는가?

---

# 5. 캐릭터 및 유닛 설계

## 5.1 플레이어블 장수

플레이어는 `ACharacter` 기반으로 구현한다.

### 역할

- 직접 조작 가능한 핵심 캐릭터
- 정교한 충돌, 피격, 애니메이션, 카메라, 입력 필요
- GAS 기반 공격/스킬 사용
- 주변 병사와 상호작용
- 거점 점령에 직접 기여

### 주요 기능

- 이동, 점프, 카메라 회전의 기본 ACharacter 기능
- 기본 공격
- 강화 공격
- 구르기 기능
- 피격
- 사망
- 거점 점령
- 주변 적 탐지
- 8방향 슬롯 전투 시스템의 중심 대상

### 구현 방식

| 항목 | 방식 |
| --- | --- |
| Base Class | `ACharacter` |
| Input | Enhanced Input |
| Ability | GAS |
| Animation | Animation Blueprint |
| Movement | Character Movement Component |
| Camera | Third Person Camera |
| Attribute | Health, AttackPower, Defense, Stamina |

---

## 5.2 장수

장수도 `ACharacter` 기반으로 구현한다.

### 역할

- 플레이어와 직접 전투하는 주요 적 또는 플레이어의 아군
- 일반 병사보다 높은 체력과 공격력
- 8방향 슬롯 전투 시스템의 대상

### 장수 AI

장수는

```
ACharacter + AIController + Behavior Tree + Blackboard
```

기반으로 구현한다.

장수는 병사처럼 대량 처리 대상이 아니라, 플레이어와 직접 상호작용하는 고중요도 캐릭터이므로 복잡한 전투 판단, 거리 유지, 패턴 선택, 스킬 사용, 후퇴, 주변 병사 호출 등을 BT로 구성한다.

### 기본 상태

```
stateDiagram-v2
    [*] --> Idle
    Idle --> Patrol
    Patrol --> ApproachPlayer
    ApproachPlayer --> Combat
    Combat --> SkillAttack
    Combat --> Retreat
    SkillAttack --> Combat
    Retreat --> ApproachPlayer
    Combat --> Dead
```

### 주요 AI 행동

- 플레이어 탐지
- 일정 거리 이내 접근
- 공격 가능 거리 유지
- 기본 공격
- 체력 낮을 때 후퇴
- 주변 병사 지원 요청

---

## 5.3 일반 병사

일반 병사는 기본적으로 `Mass Entity` 기반으로 구현한다.

단, 플레이어 주변에서 정교한 상호작용이 필요한 병사 일부는 일시적으로 `ACharacter` 또는 경량 Actor로 승격할 수 있다.

### 병사 역할

- 전장을 채우는 대규모 유닛
- 거점 공격/방어
- 병사 대 병사 전투
- 플레이어 주변에서 피격/사망 연출 제공

### 병사 구현 방식

| 거리/중요도 | 구현 방식 | 설명 |
| --- | --- | --- |
| 원거리 | Mass Entity | 위치, 체력, 소속, 목표만 계산 |
| 중거리 | Mass Entity + 단순 애니메이션 | 이동/공격 방향 정도만 계산 |
| 근거리 | Actor 또는 Character 승격 | 충돌, 피격, 애니메이션 정교화 |
| 중요 병사 | Actor 유지 | 이벤트성 병사, 튜토리얼 대상 등 |

---

# 6. 전투 시스템

## 6.1 GAS 기반 전투 구조

AshDraft의 전투는 Lyra 스타일을 참고하여 GAS 기반으로 구성한다.

### 주요 구성

| 구성 요소 | 설명 |
| --- | --- |
| Gameplay Ability | 공격, 강화 공격, 구르기(회피) |
| Gameplay Effect | 피해, 버프, 디버프 |
| Attribute Set | 체력, 공격력, 방어력, 스태미나 |
| Gameplay Tag | 상태, 공격 타입, 진영, 제어 불가 상태 |
| Ability Task | 몽타주 재생, 타겟 탐지, 히트 이벤트 처리 |

### 예시 Gameplay Tags

```
State.Dead
State.Stunned
State.Invincible
State.Attacking
State.Capturing

Team.Player
Team.Ally
Team.Enemy
Team.Neutral

Ability.Attack.Basic
Ability.Attack.Heavy
Ability.Dodge
Ability.Command.Rally

Damage.Physical
Damage.Knockback
```

---

## 6.2 8방향 슬롯 기반 근접 전투

플레이어와 적 장수 주변에는 8개의 전투 슬롯이 존재한다.

병사들은 대상에게 무작정 겹쳐서 접근하지 않고, 비어 있는 슬롯을 점유한 뒤 공격한다.

### 슬롯 방향

```
        [N]
   [NW]     [NE]

[W]   Target   [E]

   [SW]     [SE]
        [S]
```

### 슬롯 시스템 목적

- 병사들이 플레이어에게 겹쳐 붙는 문제 방지
- 전투 장면의 가독성 향상
- 무쌍류 액션의 “둘러싸임” 연출 구현
- 근접 전투 대상 수 제한
- 공격 타이밍 제어

### 슬롯 상태

| 상태 | 설명 |
| --- | --- |
| Empty | 비어 있음 |
| Reserved | 병사가 이동 중 |
| Occupied | 병사가 슬롯 도착 |
| Attacking | 공격 중 |
| Cooldown | 공격 후 대기 |

### 슬롯 점유 흐름

```
flowchart TD
    A[병사: 공격 대상 탐지] --> B[대상 주변 슬롯 요청]
    B --> C{빈 슬롯 존재? && 도달 가능?}
    C -->|Yes| D[가장 가까운 슬롯 예약]
    D --> E[슬롯 위치로 이동]
    E --> F[공격 가능 거리 도달]
    F --> G[공격 실행]
    G --> H[쿨다운]
    H --> I[슬롯 유지 또는 이탈]
    C -->|No| J[주변 대기 또는 다른 목표 탐색]
```

### 구현 컴포넌트

```
UCombatSlotComponent
- SlotOwner
- SlotRadius
- SlotCount = 8
- SlotStates
- RequestSlot()
- ReleaseSlot()
- GetSlotWorldLocation()
- FindBestAvailableSlot()
```

---

# 7. 거점 시스템

## 7.1 거점 개요

거점은 전장의 흐름을 결정하는 핵심 오브젝트이다.

각 거점은 소유 진영, 점령 게이지, 주둔 병력, 스폰 기능을 가진다.

## 7.2 거점 상태

| 상태 | 설명 |
| --- | --- |
| AllyControlled | 아군 점령 |
| EnemyControlled | 적군 점령 |
| Contested | 양 진영 병력이 동시에 존재 |

## 7.3 점령 규칙

- 거점은 장수가 병사와 체력을 보충하는 장소. 플레이어 또한 거점 반경 내에 들어오면 체력을 서서히 회복.
- 거점은 내구력 시스템이 존재함. 거점에는 거점 방어 병사들이 있음.
- 거점 방어 병사들이 1명 처치될 때마다 내구력이 1씩 감소함.
- 내구력이 0이 된다면 상대 진영으로 거점 소유권이 변경.
- 소유권이 변경된 후 약 10초 뒤 해당 진영의 거점 방어 병사들이 출몰하고 내구력이 초기화.
- 거점 내 적이 아무도 없을 경우 내구력은 초당 1씩 회복.
- 양 진영 병력이 동시에 있으면 Contested 상태. 이 상태에서는 내구력이 회복되지 않음.
- 소유권 변경(점령) 이벤트 발생 시 장수들에 알려서 목표 업데이트에 포함.

---

# 8. AI 시스템 설계

## 8.1 전체 AI 목표

AshDraft의 AI는 모든 병사가 독립적으로 사고하는 구조가 아니다.

대규모 전장을 효율적으로 처리하기 위해 다음 구조를 사용한다.

- 사령관 AI
- 분대장 AI
- 병사 AI
- AI LOD
- Time Slicing
- Mass Entity
- Flow Field Navigation

---

## 8.2 계층적 AI 구조

```
flowchart TD
    A[Battle Director / Commander AI] --> B[Ally Army Controller]
    A --> C[Enemy Army Controller]

    B --> D[Ally Squad 01]
    B --> E[Ally Squad 02]

    C --> F[Enemy Squad 01]
    C --> G[Enemy Squad 02]

    D --> H[Soldier Entities]
    E --> I[Soldier Entities]
    F --> J[Soldier Entities]
    G --> K[Soldier Entities]
```

## 8.3 Commander AI

Commander AI는 전장 전체의 전략 목표를 결정한다.

### 역할

- 현재 거점 소유 상태 분석
- 공격/방어 목표 결정
- 분대에 명령 전달
- 전장 흐름 제어
- PoC에서는 단순 StateTree로 구현
- 특정 이벤트 발생마다 의사결정 수행(거점 점령, 장수 처치 이벤트 등)

### 예시 명령

```
Order.AttackBase
Order.DefendBase
Order.CapturePoint
Order.ReinforceHero
Order.Retreat
```

---

## 8.4 Squad AI

Squad AI는 병사 개개인이 아니라 병사 그룹을 제어한다.

### 역할

- 분대 목표 거점 설정
- 분대 이동 방향 계산
- 대형 유지
- 병사 전투 상태 관리
- 병사 밀집 방지
- 사망/이탈 병사 재정렬

### 분대 데이터

```
SquadId
TeamId
CurrentOrder
TargetBaseId
AveragePosition
AliveUnitCount
Morale
FormationType
LODLevel
```

---

## 8.5 Soldier AI

일반 병사는 단순한 로컬 규칙만 수행한다.

### 병사 로컬 규칙

- 분대 목표 방향으로 이동
- 대형 위치 유지
- 근처 적 감지
- 공격 가능 거리면 공격
- 너무 밀집되면 살짝 회피
- 체력 0이면 사망 처리

병사는 전체 전술 판단을 하지 않는다.

---

# 9. AI LOD 설계

## 9.1 AI LOD 목적

플레이어에게 중요한 AI만 정교하게 계산하고, 멀리 있는 AI는 단순화한다.

## 9.2 LOD 단계

| LOD | 대상 | 처리 방식 |
| --- | --- | --- |
| LOD 0 | 플레이어 주변 10m | 정교한 충돌, 공격, 피격, 슬롯 전투 |
| LOD 1 | 플레이어 주변 30m | 단순 이동, 단순 공격, 낮은 빈도 피격 |
| LOD 2 | 원거리 전장 | 분대 단위 위치/체력만 계산 |
| LOD 3 | 카메라 밖/매우 원거리 | 수식 기반 전투 시뮬레이션 |

## 9.3 LOD 전환 규칙

```
DistanceToPlayer
CameraVisibility
ImportanceScore
IsNearBase
IsNearHero
IsRecentlyDamaged
```

### Importance Score 예시

```
Importance =
    DistanceWeight
  + VisibilityWeight
  + CombatWeight
  + HeroProximityWeight
  + ObjectiveWeight
```

## 9.4 하이브리드 Entity 전환

```
flowchart TD
    A[Mass Soldier Entity] --> B{플레이어 근처?}
    B -->|No| C[Mass 상태 유지]
    B -->|Yes| D[Actor/Character Proxy Spawn]
    D --> E[정교한 피격/전투 처리]
    E --> F{멀어졌는가?}
    F -->|Yes| G[Proxy Despawn]
    G --> H[Mass Entity로 상태 환원]
    F -->|No| E
```

---

# 10. Time Slicing / Tick Throttling

## 10.1 목적

모든 AI를 매 프레임 갱신하지 않고, 중요도에 따라 업데이트 주기를 분산한다.

## 10.2 업데이트 주기

| 대상 | 업데이트 빈도 |
| --- | --- |
| 플레이어 | 매 프레임 |
| 적 장수 | 30~60Hz |
| 근거리 병사 | 10~20Hz |
| 중거리 병사 | 5~10Hz |
| 원거리 분대 | 1~2Hz |
| 원거리 전투 시뮬레이션 | 1Hz 이하 |

## 10.3 처리 방식

```
Frame 1: Soldier Batch 0 갱신
Frame 2: Soldier Batch 1 갱신
Frame 3: Soldier Batch 2 갱신
Frame 4: Soldier Batch 3 갱신
...
```

## 10.4 PoC 검증 지표

- 100명 병사 FPS
- 300명 병사 FPS
- 500명 병사 FPS
- 1000명 병사 FPS
- Game Thread 사용량
- Mass Processor 처리 시간
- Actor 승격 병사 수
- 평균 프레임 시간

---

# 11. Mass Entity 설계

## 11.1 Mass Entity 사용 목적

대규모 병사를 `ACharacter`로 모두 구현하면 성능 문제가 발생한다.

따라서 일반 병사는 Mass Entity 기반으로 처리한다.

## 11.2 주요 Fragment

```
struct FAshTeamFragment
{
    int32 TeamId;
};

struct FAshHealthFragment
{
    float CurrentHealth;
    float MaxHealth;
};

struct FAshMovementFragment
{
    FVector Position;
    FVector Velocity;
    float MoveSpeed;
};

struct FAshCombatFragment
{
    int32 TargetEntityId;
    float AttackRange;
    float AttackCooldown;
    float AttackPower;
};

struct FAshSquadFragment
{
    int32 SquadId;
    int32 OrderId;
};

struct FAshLODFragment
{
    int32 LODLevel;
    float UpdateInterval;
    float LastUpdateTime;
};
```

## 11.3 주요 Processor

```
UAshMassLODProcessor
- 거리/카메라/중요도 기반 LOD 계산

UAshMassMovementProcessor
- Flow Field 방향 기반 이동 처리

UAshMassFormationProcessor
- 분대 대형 유지

UAshMassCombatProcessor
- 근처 적 탐지 및 단순 공격 처리

UAshMassDeathProcessor
- 체력 0 이하 병사 제거

UAshMassRepresentationProcessor
- Mass Entity 시각화 및 Actor 승격/환원 관리
```

---

# 12. Pathfinding 설계

## 12.1 기본 방침

플레이어, 장수, 중요 NPC는 NavMesh 기반 이동을 사용한다.

일반 병사 다수는 Flow Field 또는 ZoneGraph 기반 이동을 사용한다.

## 12.2 대상별 이동 방식

| 대상 | 이동 방식 |
| --- | --- |
| 플레이어 | 직접 입력 |
| 적 장수 | NavMesh |
| 근거리 Actor 병사 | NavMesh 또는 단순 Steering |
| Mass 병사 | Flow Field |
| 원거리 분대 | 추상화된 거점 그래프 이동 |

## 12.3 Flow Field Navigation

Flow Field는 특정 목표 지점에 대해 맵 격자별 이동 방향을 미리 계산한다.

### 목적

- 병사 개별 A* 연산 제거
- 동일 목표를 향하는 부대의 이동 비용 절감
- 거점 공격/방어 이동 최적화

### Flow Field 구조

```
Target: CentralBase

Grid Cell:
- Cost
- BestDirection
- Blocked
```

### 이동 예시

```
SoldierPosition -> CurrentGridCell -> BestDirection -> Move
```

---

# 13. Lyra 스타일 프로젝트 구조

## 13.1 기본 원칙

AshDraft는 Lyra 스타일 구조를 참고한다.

핵심 원칙은 다음과 같다.

- Game Feature Plugin 기반 기능 분리
- Pawn Data 기반 캐릭터 설정
- Experience Definition 기반 게임 모드 구성
- GAS 기반 능력 시스템
- Gameplay Tag 중심 상태 관리
- Enhanced Input 기반 입력 처리
- 데이터 에셋 중심 설계

## 13.2 권장 모듈 구조

```
Source/
  AshDraft/
    Core/
    Character/
    AbilitySystem/
    Combat/
    AI/
    Mass/
    Base/
    UI/
    GameModes/
    System/

Plugins/
  GameFeatures/
    AshDraftCore/
    AshDraftHero/
    AshDraftArmy/
    AshDraftBaseSystem/
    AshDraftBattleAI/
```

## 13.3 Game Feature Plugin 분리

| Plugin | 역할 |
| --- | --- |
| AshDraftCore | 공통 태그, 공통 데이터, 기본 시스템 |
| AshDraftHero | 플레이어 장수, 입력, 카메라, 능력 |
| AshDraftArmy | 병사, 분대, Mass Entity |
| AshDraftBaseSystem | 거점 점령, 스폰, 소유권 |
| AshDraftBattleAI | Commander AI, Squad AI, AI LOD |

---

# 14. 전체 아키텍처 구조도

```
flowchart TD
    A[Player Input] --> B[Playable Hero ACharacter]
    B --> C[GAS Ability System]
    C --> D[Combat / Damage System]

    E[Battle Director] --> F[Commander AI]
    F --> G[Squad AI]
    G --> H[Mass Soldier Entities]

    H --> I[Mass Processors]
    I --> J[LOD Processor]
    I --> K[Movement Processor]
    I --> L[Combat Processor]
    I --> M[Representation Processor]

    N[Base System] --> F
    N --> G
    N --> O[Spawn System]

    P[Flow Field / Nav Data] --> K
    Q[NavMesh] --> R[Hero / General AI]

    S[Telemetry Interface] --> T[Future RL QA Bot]
    B --> S
    N --> S
    F --> S
```

---

# 15. 주요 구현 기능 목록

## 15.1 플레이어 장수

- 이동
- 카메라
- 기본 공격
- 피격
- 체력
- 사망
- 거점 점령 기여
- GAS Ability 적용

## 15.2 적 장수

- 플레이어 추적
- 공격
- 피격
- 체력
- 사망
- 주변 병사 지원
- StateTree 기반 상태 전환

## 15.3 일반 병사

- Mass Entity 스폰
- 분대 소속
- 이동
- 대형 유지
- 단순 공격
- 사망
- LOD 전환
- Actor Proxy 승격/환원

## 15.4 거점

- 소유권
- 점령 게이지
- 진영별 병력 감지
- 스폰 포인트
- 점령 완료 이벤트
- AI 목표 갱신 이벤트

## 15.5 전장 AI

- Commander AI
- Squad AI
- 병사 로컬 AI
- AI LOD
- Time Slicing
- Flow Field Movement

## 15.6 QA Bot 확장 인터페이스

1차 PoC에서는 실제 강화학습을 구현하지 않는다.

대신 향후 QA Bot이 붙을 수 있도록 상태/액션 인터페이스를 설계한다.

### 상태 Observation 예시

```
{
  "player_health": 85,
  "player_position": [120.0, 40.0, 0.0],
  "nearby_enemy_count": 12,
  "owned_bases": 1,
  "enemy_bases": 2,
  "current_objective": "CaptureCentralBase"
}
```

### Action 예시

```
{
  "move": [1.0, 0.0],
  "camera_yaw": 15.0,
  "attack": true,
  "dodge": false,
  "skill_1": false
}
```

---

# 16. 멀티플레이 확장 고려

1차 PoC는 싱글플레이로 제작한다.

하지만 향후 멀티플레이 확장을 위해 다음 원칙을 지킨다.

## 16.1 Authority 원칙

- 게임 핵심 판정은 Server Authority 기준으로 설계
- 클라이언트는 입력과 표현에 집중
- 피해, 사망, 거점 점령은 서버 권한으로 처리 가능한 구조 유지

## 16.2 GAS 설계

- Gameplay Ability는 Predict 가능하도록 구조화
- Gameplay Effect는 서버 기준 적용 가능하게 설계
- Gameplay Tag는 네트워크 동기화 가능한 상태 표현에 사용

## 16.3 Mass Entity 멀티플레이 고려

대규모 병사를 모두 네트워크 복제하지 않는다.

향후 방향은 다음과 같다.

- 플레이어 주변 중요 병사만 복제
- 원거리 병사는 클라이언트 예측 또는 시뮬레이션 결과만 동기화
- 병사 개체 단위가 아니라 분대/거점 상태 단위 동기화
- 전장 흐름은 서버에서 계산하고 클라이언트는 시각화

---

# 17. 향후 강화학습 QA Bot 설계 방향

## 17.1 목표

향후 강화학습 기반 QA Bot은 플레이어처럼 게임을 플레이하면서 다음을 검증한다.

- 전투 난이도
- 거점 점령 가능성
- 맵 동선 문제
- 병사 AI 병목
- 플레이어 스킬 밸런스
- 특정 상황에서 클리어 불가능한 문제

## 17.2 1차 PoC에서 준비할 것

- 플레이어 입력 추상화
- 게임 상태 Observation 추출
- 전투 로그 기록
- 거점 상태 로그 기록
- 승리/패배 조건 이벤트화
- Headless 또는 빠른 시뮬레이션 모드 가능성 검토

## 17.3 RL 환경 인터페이스 초안

```
Reset()
Step(Action)
GetObservation()
GetReward()
IsDone()
```

## 17.4 Reward 예시

```
+10 거점 점령
+50 적 장수 처치
+100 승리
-10 플레이어 큰 피해
-100 사망
-1 시간 경과
```

---

# 18. 코드 규칙

## 18.1 C++ 네이밍 규칙

Unreal Engine 기본 네이밍 규칙을 따른다.

| 종류 | 규칙 | 예시 |
| --- | --- | --- |
| Actor | A 접두사 | `AAshHeroCharacter` |
| Component | U 접두사 | `UAshCombatSlotComponent` |
| Struct | F 접두사 | `FAshSoldierFragment` |
| Enum | E 접두사 | `EAshTeamId` |
| Interface | I 접두사 | `IAshDamageableInterface` |
| Data Asset | U 접두사 | `UAshHeroPawnData` |

## 18.2 클래스명 규칙

모든 게임 전용 클래스는 `Ash` 접두사를 사용한다.

```
AAshHeroCharacter
AAshEnemyGeneralCharacter
AAshBaseActor
UAshAbilitySystemComponent
UAshCombatSlotComponent
UAshMassMovementProcessor
UAshCommanderSubsystem
```

## 18.3 파일 구조 규칙

하나의 클래스는 하나의 `.h`, `.cpp` 파일로 분리한다.

```
AshHeroCharacter.h
AshHeroCharacter.cpp
AshCombatSlotComponent.h
AshCombatSlotComponent.cpp
```

## 18.4 Blueprint 규칙

Blueprint 자산은 접두사를 사용한다.

| 자산 | 접두사 | 예시 |
| --- | --- | --- |
| Blueprint Actor | BP_ | `BP_Hero_LiuBei` |
| Animation Blueprint | ABP_ | `ABP_Hero` |
| Widget Blueprint | WBP_ | `WBP_BaseStatus` |
| Data Asset | DA_ | `DA_HeroPawnData` |
| Gameplay Ability | GA_ | `GA_Hero_BasicAttack` |
| Gameplay Effect | GE_ | `GE_Damage_Physical` |
| Gameplay Cue | GC_ | `GC_HitImpact` |
| Input Mapping Context | IMC_ | `IMC_Hero` |
| Input Action | IA_ | `IA_Attack` |

## 18.5 Gameplay Tag 규칙

Gameplay Tag는 계층적으로 관리한다.

```
Ash.State.Dead
Ash.State.Stunned
Ash.State.Attacking

Ash.Team.Player
Ash.Team.Ally
Ash.Team.Enemy

Ash.Ability.Attack.Basic
Ash.Ability.Attack.Heavy

Ash.Objective.Base.Capture
Ash.Objective.Base.Defend
```

## 18.6 Tick 사용 규칙

- 기본적으로 Actor Tick 사용 금지
- 꼭 필요한 경우에만 Tick 활성화
- Mass 병사는 Processor에서 일괄 처리
- AI 업데이트는 Time Slicing 사용
- 디버그 Tick과 런타임 Tick 분리

## 18.7 의존성 규칙

- UI가 AI를 직접 참조하지 않는다.
- AI가 UI를 직접 참조하지 않는다.
- Mass Entity가 Player Character를 직접 강참조하지 않는다.
- 전역 접근은 Subsystem을 통해 관리한다.
- Gameplay Tag와 Data Asset을 우선 사용한다.
- 하드코딩된 수치 대신 Config/Data Asset 사용을 우선한다.

---

# 19. Git 컨벤션

## 19.1 브랜치 전략

```
main
  안정 버전

develop
  통합 개발 브랜치

feature/*
  기능 개발 브랜치

fix/*
  버그 수정 브랜치

tech/*
  기술 실험 브랜치

docs/*
  문서 작업 브랜치
```

## 19.2 브랜치 예시

```
feature/hero-basic-combat
feature/base-capture-system
feature/mass-soldier-spawn
feature/combat-slot-system
tech/flow-field-navigation
tech/mass-actor-hybrid-lod
docs/poc-design-document
```

## 19.3 커밋 메시지 규칙

형식:

```
type(scope): summary
```

예시:

```
feat(hero): add basic attack ability
feat(base): implement capture gauge
feat(mass): add soldier movement processor
fix(combat): prevent duplicate slot reservation
refactor(ai): split commander and squad logic
docs(poc): update architecture diagram
test(base): add capture calculation test
```

## 19.4 커밋 타입

| 타입 | 의미 |
| --- | --- |
| feat | 기능 추가 |
| fix | 버그 수정 |
| refactor | 구조 개선 |
| perf | 성능 개선 |
| docs | 문서 수정 |
| test | 테스트 추가 |
| chore | 빌드/설정/기타 |
| art | 에셋/시각 자료 변경 |
| proto | 실험성 프로토타입 |

## 19.5 Pull Request 규칙

PR에는 다음 내용을 포함한다.

```
## Summary
- 변경 요약

## Changes
- 주요 변경 사항

## Test
- 테스트 방법
- 확인한 맵/상황

## Screenshot / Video
- 필요 시 첨부

## Risk
- 예상되는 문제
```

---

# 20. 개발 마일스톤

## Milestone 1: 프로젝트 기반 구축

목표:

- UE5 프로젝트 생성
- Lyra 스타일 폴더 구조 구성
- 기본 Game Feature Plugin 구성
- Gameplay Tag 초기 구성
- Enhanced Input 구성

완료 기준:

- 플레이어 캐릭터가 맵에서 이동 가능
- 기본 입력 동작
- 기본 카메라 동작

---

## Milestone 2: 플레이어 전투 구현

목표:

- GAS 기반 기본 공격
- 체력 Attribute
- 피해 Gameplay Effect
- 피격 반응
- 사망 처리

완료 기준:

- 플레이어가 더미 적을 공격 가능
- 피해 수치가 Attribute에 반영
- 체력 0이면 사망 처리

---

## Milestone 3-1: 장수 구현

목표:

- 장수 Character 구현
- BT 기반 추적/공격
- 피격/사망 처리
- 5단계 사기 시스템, 등급에 따라 능력치에 가중치 적용
- 이벤트 발생에 따라 사기 시스템 영향

완료 기준:

- 적 장수가 플레이어를 추적
- 공격 거리에서 공격
- 플레이어 공격에 반응
- 아군끼리의 협동
- 진영 구분에 따른 적/아군 인식

---

### Milestone 3-2:  분대 AI 구현

목표:

- 분대 정렬, 분대 목표 설정
- 분대 내 개별 장수, 병사들이 분대를 이탈하지 않도록 조정
- StateTree 기반의 거점 점령/거점 방어/적 처치/아군 구원 등의 작업 수행
- 분대에 접근하면 해당 분대의 체력바 UI가 나타남. 병사 수에 비례. 해당 체력에 따라 후퇴 등을 결정

완료 기준:

- 주요 이벤트에 따라 의사결정이 수행되는지
- 분대를 적절하게 제어할 수 있는지

---

## Milestone 4: 8방향 슬롯 전투 구현

목표:

- Combat Slot Component 구현
- 슬롯 예약/점유/해제
- 병사 또는 더미 적이 슬롯으로 이동 후 공격

완료 기준:

- 대상 주변에 병사들이 겹치지 않고 배치
- 빈 슬롯이 없으면 대기
- 공격 후 슬롯 유지 또는 해제

---

## Milestone 5: 거점 시스템 구현

목표:

- Base Actor 구현
- 내구도 시스템
- 소유권 변경
- UI 표시
- 거점 이벤트 발생

완료 기준:

- 플레이어와 장수가 거점 점령 가능
- 소유권 변경 시 색상/상태 변경
- AI 목표 갱신 이벤트 발생

---

## Milestone 6: Mass 병사 구현

목표:

- Mass Entity 병사 스폰
- Team/Health/Movement/Combat Fragment 구성
- Movement Processor
- Combat Processor

완료 기준:

- 수백 명 병사 스폰 가능
- 병사가 목표 방향으로 이동
- 단순 전투 처리 가능

---

## Milestone 7: 계층적 AI 구현

목표:

- Commander AI
- Squad AI
- 분대 명령
- 거점 기반 목표 결정

완료 기준:

- 거점 상태에 따라 분대 목표 변경
- 병사들이 분대 단위로 이동
- 모든 병사가 개별 길찾기를 하지 않음

---

## Milestone 8: AI LOD / Time Slicing 구현

목표:

- 거리 기반 LOD
- 업데이트 주기 분산
- 원거리 전투 수식 처리
- Actor Proxy 승격/환원 실험

완료 기준:

- 플레이어 주변 병사만 정교하게 동작
- 원거리 병사는 낮은 빈도로 처리
- 병사 수 증가에 따른 FPS 측정 가능

---

## Milestone 9: Flow Field 이동 PoC

목표:

- 거점 목표별 Flow Field 생성
- Mass 병사 이동에 적용
- NavMesh 방식과 비교

완료 기준:

- 병사들이 Flow Field 방향을 따라 이동
- A* 개별 호출 없이 집단 이동 가능
- 병목 지형에서 이동 안정성 확인

---

## Milestone 10: QA Bot 인터페이스 준비

목표:

- Observation 추출
- Action 입력 추상화
- 전투/거점 로그 기록
- 승패 이벤트 기록

완료 기준:

- 외부 봇 또는 자동 입력기가 플레이어 입력을 대체 가능
- 상태를 JSON 형태로 추출 가능
- 플레이 결과 로그 저장 가능

---

# 21. PoC 성공 기준

## 21.1 기능 성공 기준

- 플레이어가 전장 맵에서 이동/공격 가능
- 적 장수와 전투 가능
- 병사들이 거점 목표를 향해 이동
- 거점 점령 가능
- 거점 소유권에 따라 전장 흐름 변경
- 장수 주변 8방향 슬롯 전투 동작
- 병사들이 겹치지 않고 전투 참여

## 21.2 기술 성공 기준

- 300명 이상 병사에서 안정적인 프레임 유지
- 500명 이상 병사에서 Mass Entity 구조 검증
- ACharacter만 사용한 방식 대비 성능 이점 확인
- AI LOD 적용 전/후 성능 차이 측정
- Time Slicing 적용 전/후 Game Thread 비용 비교
- Flow Field 이동 가능성 확인
- 향후 RL QA Bot 연동을 위한 상태/입력 인터페이스 확보

## 21.3 포트폴리오 성공 기준

PoC 완료 후 다음 내용을 포트폴리오에 제시할 수 있어야 한다.

- Lyra 스타일 게임 아키텍처 적용 경험
- GAS 기반 전투 시스템 설계 경험
- Mass Entity 기반 대규모 병사 시스템 구현 경험
- 계층적 AI 구조 설계 경험
- AI LOD 및 Time Slicing 최적화 경험
- Flow Field Navigation 실험 경험
- 강화학습 QA Bot 확장을 고려한 인터페이스 설계 경험

---

# 22. 핵심 설계 원칙 요약

## 22.1 플레이어와 장수는 정교하게

플레이어와 적 장수는 `ACharacter`, GAS, Animation Blueprint, 정교한 충돌과 피격을 사용한다.

## 22.2 병사는 가볍게

일반 병사는 Mass Entity 기반으로 처리한다.

개별 병사가 복잡한 판단을 하지 않도록 한다.

## 22.3 판단은 계층적으로

사령관이 전략 목표를 정하고, 분대장이 이동과 대형을 관리하며, 병사는 로컬 규칙만 수행한다.

## 22.4 가까운 것만 진짜처럼

플레이어 주변 병사만 정교하게 동작한다.

원거리 병사는 수식과 집단 상태로 처리한다.

## 22.5 처음부터 멀티플레이를 완성하지 않는다

1차 PoC는 싱글플레이로 만든다.

다만 권한 구조, GAS, 상태 동기화 단위는 향후 멀티플레이를 망치지 않도록 설계한다.

## 22.6 강화학습은 나중에 붙인다

1차 PoC에서 RL을 직접 학습하지 않는다.

대신 Observation, Action, Reward, Log 구조를 미리 만들어 향후 QA Bot으로 확장 가능하게 한다.

---

# 23. 최종 한 줄 정의

AshDraft 1차 PoC는 “한 명의 장수가 수백 명의 병사와 거점을 두고 싸우는 전장”을 구현하면서, Lyra/GAS 기반 캐릭터 구조와 Mass Entity 기반 대규모 병력 AI를 결합하는 기술 검증 프로젝트이다.