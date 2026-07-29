/**
 * 第一人称控制器
 * 负责：WASD 移动、鼠标视角（PointerLockControls）、
 *       Shift 疾跑、空格跳跃、重力、障碍物碰撞检测
 */

import * as THREE from 'three';
import { PointerLockControls } from 'three/addons/controls/PointerLockControls.js';
import type { InputState, AABB } from '../types';
import { CONFIG } from '../types';

export class Player {
  /** PointerLock 控制器（挂载到 camera） */
  readonly controls: PointerLockControls;

  /** 玩家当前生命值 */
  health: number = CONFIG.playerMaxHealth;

  /** 受伤后的无敌帧计时（防止连续扣血） */
  private _invincibleTimer: number = 0;

  /** Y 轴速度（用于重力/跳跃） */
  private _velocityY: number = 0;

  /** 是否在地面上 */
  private _onGround: boolean = true;

  /** 水平移动速度向量（XZ 平面） */
  private _velocity: THREE.Vector3 = new THREE.Vector3();

  /** 临时向量，避免每帧 new */
  private _dir: THREE.Vector3 = new THREE.Vector3();

  constructor(
    private readonly camera: THREE.Camera,
    private readonly renderer: THREE.WebGLRenderer,
  ) {
    this.controls = new PointerLockControls(camera, renderer.domElement);
    // 初始位置：场景中心稍偏，眼睛高度
    camera.position.set(0, CONFIG.eyeHeight, 5);
  }

  /** 每帧更新：应用移动、重力、碰撞 */
  update(dt: number, input: InputState, obstacles: AABB[]): void {
    if (this._invincibleTimer > 0) {
      this._invincibleTimer -= dt;
    }

    this._applyMovement(dt, input);
    this._applyGravity(dt, input);
    this._resolveCollisions(obstacles);
  }

  /** 应用水平移动 */
  private _applyMovement(dt: number, input: InputState): void {
    const speed = input.sprint ? CONFIG.sprintSpeed : CONFIG.walkSpeed;

    // 获取摄像机朝向（忽略 Y 轴）
    this.camera.getWorldDirection(this._dir);
    this._dir.y = 0;
    this._dir.normalize();

    // 右方向
    const right = new THREE.Vector3();
    right.crossVectors(this._dir, new THREE.Vector3(0, 1, 0)).normalize();

    this._velocity.set(0, 0, 0);

    if (input.forward) this._velocity.addScaledVector(this._dir, speed);
    if (input.backward) this._velocity.addScaledVector(this._dir, -speed);
    if (input.left) this._velocity.addScaledVector(right, -speed);
    if (input.right) this._velocity.addScaledVector(right, speed);

    // 限速（对角移动不超速）
    if (this._velocity.length() > speed) {
      this._velocity.setLength(speed);
    }

    const pos = this.camera.position;
    pos.x += this._velocity.x * dt;
    pos.z += this._velocity.z * dt;
  }

  /** 应用重力与跳跃 */
  private _applyGravity(dt: number, input: InputState): void {
    // 跳跃
    if (input.jump && this._onGround) {
      this._velocityY = CONFIG.jumpSpeed;
      this._onGround = false;
    }

    // 重力加速
    this._velocityY -= CONFIG.gravity * dt;

    const pos = this.camera.position;
    pos.y += this._velocityY * dt;

    // 地面碰撞
    if (pos.y <= CONFIG.eyeHeight) {
      pos.y = CONFIG.eyeHeight;
      this._velocityY = 0;
      this._onGround = true;
    }
  }

  /** 与障碍物 AABB 进行简单轴向碰撞分离 */
  private _resolveCollisions(obstacles: AABB[]): void {
    const pos = this.camera.position;
    const r = CONFIG.playerRadius;
    const eyeY = pos.y;
    const feetY = eyeY - CONFIG.eyeHeight;

    for (const box of obstacles) {
      // Y 轴范围重叠判断
      if (feetY > box.maxY || eyeY < box.minY) continue;

      // XZ 圆与 AABB 最近点距离判断
      const nearX = Math.max(box.minX, Math.min(pos.x, box.maxX));
      const nearZ = Math.max(box.minZ, Math.min(pos.z, box.maxZ));
      const dx = pos.x - nearX;
      const dz = pos.z - nearZ;
      const distSq = dx * dx + dz * dz;

      if (distSq < r * r) {
        // 推开玩家
        const dist = Math.sqrt(distSq) || 0.001;
        const pen = r - dist;
        pos.x += (dx / dist) * pen;
        pos.z += (dz / dist) * pen;
      }
    }
  }

  /** 玩家受伤 */
  takeDamage(amount: number): void {
    if (this._invincibleTimer > 0) return;
    this.health = Math.max(0, this.health - amount);
    // 短暂无敌帧
    this._invincibleTimer = 0.3;
  }

  /** 重置玩家（重新开始游戏） */
  reset(): void {
    this.health = CONFIG.playerMaxHealth;
    this._velocityY = 0;
    this._onGround = true;
    this._invincibleTimer = 0;
    this.camera.position.set(0, CONFIG.eyeHeight, 5);
    this.camera.rotation.set(0, 0, 0);
  }

  /** 是否已死亡 */
  get isDead(): boolean {
    return this.health <= 0;
  }

  /** 生命值百分比 0~1 */
  get healthRatio(): number {
    return this.health / CONFIG.playerMaxHealth;
  }
}
