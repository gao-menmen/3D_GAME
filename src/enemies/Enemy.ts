/**
 * 敌人实体与基础 AI
 * 所有模型为程序化几何体，无第三方版权资源
 * AI 行为：朝玩家移动 → 进入攻击范围后攻击玩家
 */

import * as THREE from 'three';
import type { AABB } from '../types';
import { CONFIG } from '../types';

/** 单个敌人实体 */
export class Enemy {
  /** 敌人根节点（胶囊形状：圆柱 + 两个半球） */
  readonly mesh: THREE.Group;

  /** 当前生命值 */
  health: number = CONFIG.enemyHealth;

  /** 是否已死亡 */
  isDead: boolean = false;

  /** 攻击冷却 */
  private _attackTimer: number = 0;

  constructor(position: THREE.Vector3) {
    this.mesh = this._buildModel();
    this.mesh.position.copy(position);
  }

  /** 构建胶囊体敌人模型 */
  private _buildModel(): THREE.Group {
    const group = new THREE.Group();

    // 身体：圆柱
    const bodyGeo = new THREE.CylinderGeometry(0.35, 0.35, 1.2, 10);
    const bodyMat = new THREE.MeshLambertMaterial({ color: 0x3a5a2a });
    const body = new THREE.Mesh(bodyGeo, bodyMat);
    body.position.y = 0.9;
    body.castShadow = true;
    group.add(body);

    // 头部：球体
    const headGeo = new THREE.SphereGeometry(0.3, 10, 10);
    const headMat = new THREE.MeshLambertMaterial({ color: 0x8a6a4a });
    const head = new THREE.Mesh(headGeo, headMat);
    head.position.y = 1.8;
    head.castShadow = true;
    group.add(head);

    // 头盔
    const helmetGeo = new THREE.SphereGeometry(0.32, 10, 6, 0, Math.PI * 2, 0, Math.PI * 0.55);
    const helmetMat = new THREE.MeshLambertMaterial({ color: 0x2a3a1a });
    const helmet = new THREE.Mesh(helmetGeo, helmetMat);
    helmet.position.y = 1.82;
    group.add(helmet);

    // 腿（两条）
    const legGeo = new THREE.BoxGeometry(0.2, 0.8, 0.2);
    const legMat = new THREE.MeshLambertMaterial({ color: 0x2a3520 });
    const legL = new THREE.Mesh(legGeo, legMat);
    legL.position.set(-0.2, 0.4, 0);
    legL.castShadow = true;
    group.add(legL);

    const legR = legL.clone();
    legR.position.set(0.2, 0.4, 0);
    group.add(legR);

    // 血量条背景
    const bgGeo = new THREE.PlaneGeometry(0.8, 0.08);
    const bgMat = new THREE.MeshBasicMaterial({ color: 0x330000, side: THREE.DoubleSide });
    const bg = new THREE.Mesh(bgGeo, bgMat);
    bg.position.set(0, 2.3, 0);
    group.add(bg);

    // 血量条前景
    const barGeo = new THREE.PlaneGeometry(0.8, 0.08);
    const barMat = new THREE.MeshBasicMaterial({ color: 0x00cc00, side: THREE.DoubleSide });
    const bar = new THREE.Mesh(barGeo, barMat);
    bar.position.set(0, 2.3, 0.001);
    bar.name = 'healthBar';
    group.add(bar);

    return group;
  }

  /** 每帧更新 AI 行为 */
  update(
    dt: number,
    playerPos: THREE.Vector3,
    onAttackPlayer: (damage: number) => void,
    obstacles: AABB[],
  ): void {
    if (this.isDead) return;

    // 使血量条始终朝向摄像机（Billboard）
    const bar = this.mesh.getObjectByName('healthBar');
    if (bar !== undefined) {
      bar.lookAt(playerPos);
    }

    const pos = this.mesh.position;
    const dx = playerPos.x - pos.x;
    const dz = playerPos.z - pos.z;
    const dist = Math.sqrt(dx * dx + dz * dz);

    // 朝玩家旋转
    this.mesh.rotation.y = Math.atan2(dx, dz);

    if (dist > CONFIG.enemyAttackRange) {
      // 移动朝向玩家
      const speed = CONFIG.enemyMoveSpeed;
      const ndx = dx / dist;
      const ndz = dz / dist;
      const newX = pos.x + ndx * speed * dt;
      const newZ = pos.z + ndz * speed * dt;

      // 简单障碍物推开（防止敌人卡进墙里）
      let blocked = false;
      for (const box of obstacles) {
        if (newX + 0.4 > box.minX && newX - 0.4 < box.maxX &&
            newZ + 0.4 > box.minZ && newZ - 0.4 < box.maxZ &&
            0 < box.maxY) {
          blocked = true;
          break;
        }
      }
      if (!blocked) {
        pos.x = newX;
        pos.z = newZ;
      }
    } else {
      // 近战攻击玩家
      this._attackTimer -= dt;
      if (this._attackTimer <= 0) {
        onAttackPlayer(CONFIG.enemyAttackDamage);
        this._attackTimer = CONFIG.enemyAttackInterval;
      }
    }
  }

  /** 受伤 */
  takeDamage(amount: number): void {
    this.health = Math.max(0, this.health - amount);
    this._updateHealthBar();
    if (this.health <= 0) {
      this.isDead = true;
    }
  }

  /** 更新血量条显示 */
  private _updateHealthBar(): void {
    const bar = this.mesh.getObjectByName('healthBar') as THREE.Mesh | undefined;
    if (bar === undefined) return;
    const ratio = Math.max(0, this.health / CONFIG.enemyHealth);
    bar.scale.x = ratio;
    // 左对齐
    bar.position.x = (ratio - 1) * 0.4;
  }

  /** 在场景中移除自身 */
  removeFrom(scene: THREE.Scene): void {
    scene.remove(this.mesh);
  }
}

/** 敌人管理器：批量创建、更新、移除 */
export class EnemyManager {
  readonly enemies: Enemy[] = [];

  constructor(private readonly scene: THREE.Scene) {}

  /** 在随机位置生成 count 个敌人 */
  spawn(count: number): void {
    const spread = 35;
    for (let i = 0; i < count; i++) {
      // 生成位置避开原点附近（玩家出生点）
      let x: number, z: number;
      do {
        x = (Math.random() - 0.5) * spread * 2;
        z = (Math.random() - 0.5) * spread * 2;
      } while (Math.sqrt(x * x + z * z) < 10);

      const enemy = new Enemy(new THREE.Vector3(x, 0, z));
      this.scene.add(enemy.mesh);
      this.enemies.push(enemy);
    }
  }

  /** 每帧更新所有存活敌人的 AI */
  update(
    dt: number,
    playerPos: THREE.Vector3,
    onAttackPlayer: (damage: number) => void,
    obstacles: AABB[],
  ): number {
    let kills = 0;
    for (let i = this.enemies.length - 1; i >= 0; i--) {
      const enemy = this.enemies[i];
      if (enemy === undefined) continue;
      if (enemy.isDead) {
        enemy.removeFrom(this.scene);
        this.enemies.splice(i, 1);
        kills++;
        continue;
      }
      enemy.update(dt, playerPos, onAttackPlayer, obstacles);
    }
    return kills;
  }

  /** 返回所有存活敌人的 mesh，供 Raycaster 使用 */
  getTargetMeshes(): THREE.Object3D[] {
    return this.enemies
      .filter(e => !e.isDead)
      .map(e => e.mesh);
  }

  /** 根据 mesh 找到对应敌人 */
  findByMesh(obj: THREE.Object3D): Enemy | undefined {
    for (const enemy of this.enemies) {
      if (obj === enemy.mesh || enemy.mesh.getObjectById(obj.id) !== undefined) {
        return enemy;
      }
    }
    return undefined;
  }

  /** 清空所有敌人（重新开始游戏） */
  clear(): void {
    for (const enemy of this.enemies) {
      enemy.removeFrom(this.scene);
    }
    this.enemies.length = 0;
  }
}
