/** 公共类型定义与全局配置常量 */

/** 游戏状态 */
export type GameState = 'menu' | 'playing' | 'paused' | 'dead';

/** 每帧玩家输入快照 */
export interface InputState {
  forward: boolean;
  backward: boolean;
  left: boolean;
  right: boolean;
  jump: boolean;
  sprint: boolean;
  shooting: boolean;
  reload: boolean;
}

/** HUD 每帧需要展示的数据 */
export interface HudData {
  health: number;
  maxHealth: number;
  magAmmo: number;
  reserveAmmo: number;
  reloading: boolean;
  kills: number;
}

/** 障碍物轴对齐包围盒（AABB），用于碰撞检测 */
export interface AABB {
  minX: number;
  maxX: number;
  minZ: number;
  maxZ: number;
  minY: number;
  maxY: number;
}

/** 全局可调参数 */
export const CONFIG = {
  worldSize: 160,         // 世界地面尺寸（米）
  gravity: 24,            // 重力加速度（m/s²）
  eyeHeight: 1.7,         // 玩家眼睛高度（m）
  walkSpeed: 6,           // 行走速度（m/s）
  sprintSpeed: 10.5,      // 疾跑速度（m/s）
  jumpSpeed: 8.5,         // 跳跃初速度（m/s）
  playerRadius: 0.45,     // 玩家碰撞半径（m）
  playerMaxHealth: 100,   // 玩家最大生命值
  enemyCount: 8,          // 初始敌人数量
  enemyMoveSpeed: 2.5,    // 敌人移动速度（m/s）
  enemyAttackRange: 2.2,  // 敌人近战攻击范围（m）
  enemyAttackDamage: 12,  // 敌人每次攻击伤害
  enemyAttackInterval: 1, // 敌人攻击间隔（s）
  enemyHealth: 80,        // 敌人生命值
  magSize: 30,            // 弹匣容量
  reserveAmmo: 120,       // 备弹数量
  fireRate: 0.1,          // 射速（s / 发）
  reloadTime: 2.2,        // 换弹时间（s）
  bulletDamage: 25,       // 每发子弹伤害
  recoilAmount: 0.018,    // 后坐力大小（弧度）
  flashDuration: 0.07,    // 枪口闪光持续时间（s）
} as const;
