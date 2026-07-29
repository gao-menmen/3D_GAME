/**
 * 武器系统
 * 负责：武器模型（程序化几何体组合）、射击、Raycaster 命中判定、
 *       枪口闪光、后坐力、弹药管理、换弹
 */

import * as THREE from 'three';
import { CONFIG } from '../types';

export class Weapon {
  /** 武器模型根节点（挂载到摄像机下） */
  readonly mesh: THREE.Group;

  /** 枪口闪光精灵 */
  private _flashMesh: THREE.Mesh;

  /** 当前弹匣子弹数 */
  magAmmo: number = CONFIG.magSize;

  /** 备弹数 */
  reserveAmmo: number = CONFIG.reserveAmmo;

  /** 是否正在换弹 */
  reloading: boolean = false;

  /** 换弹剩余时间 */
  private _reloadTimer: number = 0;

  /** 射速冷却计时 */
  private _fireCooldown: number = 0;

  /** 枪口闪光剩余时间 */
  private _flashTimer: number = 0;

  /** 当前后坐力偏移（pitch） */
  private _recoilPitch: number = 0;

  /** Raycaster，复用避免每帧 new */
  private readonly _raycaster: THREE.Raycaster = new THREE.Raycaster();

  constructor(private readonly camera: THREE.Camera) {
    this.mesh = this._buildWeaponModel();
    this._flashMesh = this._buildFlashMesh();
    // 枪口闪光挂到武器 mesh 前端
    this._flashMesh.position.set(0, 0.04, -0.62);
    this._flashMesh.visible = false;
    this.mesh.add(this._flashMesh);

    // 挂载武器到摄像机（右下角视角位置）
    camera.add(this.mesh);
    this.mesh.position.set(0.22, -0.25, -0.55);
  }

  /** 构建简单突击步枪外形（纯程序化几何体） */
  private _buildWeaponModel(): THREE.Group {
    const group = new THREE.Group();

    // 枪身
    const bodyGeo = new THREE.BoxGeometry(0.06, 0.09, 0.55);
    const bodyMat = new THREE.MeshLambertMaterial({ color: 0x2a2a2a });
    const body = new THREE.Mesh(bodyGeo, bodyMat);
    group.add(body);

    // 枪管
    const barrelGeo = new THREE.CylinderGeometry(0.018, 0.018, 0.32, 8);
    const barrelMat = new THREE.MeshLambertMaterial({ color: 0x1a1a1a });
    const barrel = new THREE.Mesh(barrelGeo, barrelMat);
    barrel.rotation.x = Math.PI / 2;
    barrel.position.set(0, 0.02, -0.42);
    group.add(barrel);

    // 弹匣
    const magGeo = new THREE.BoxGeometry(0.04, 0.12, 0.07);
    const magMat = new THREE.MeshLambertMaterial({ color: 0x1c1c1c });
    const mag = new THREE.Mesh(magGeo, magMat);
    mag.position.set(0, -0.1, 0.02);
    group.add(mag);

    // 枪托
    const stockGeo = new THREE.BoxGeometry(0.05, 0.07, 0.18);
    const stockMat = new THREE.MeshLambertMaterial({ color: 0x3a2a1a });
    const stock = new THREE.Mesh(stockGeo, stockMat);
    stock.position.set(0, -0.01, 0.34);
    group.add(stock);

    // 瞄准镜
    const sightGeo = new THREE.BoxGeometry(0.025, 0.025, 0.1);
    const sightMat = new THREE.MeshLambertMaterial({ color: 0x111111 });
    const sight = new THREE.Mesh(sightGeo, sightMat);
    sight.position.set(0, 0.06, -0.08);
    group.add(sight);

    return group;
  }

  /** 构建枪口闪光面片 */
  private _buildFlashMesh(): THREE.Mesh {
    const geo = new THREE.PlaneGeometry(0.18, 0.18);
    const mat = new THREE.MeshBasicMaterial({
      color: 0xffee88,
      transparent: true,
      opacity: 1,
      depthWrite: false,
      side: THREE.DoubleSide,
    });
    return new THREE.Mesh(geo, mat);
  }

  /** 每帧更新：冷却、换弹、闪光、后坐力回正 */
  update(dt: number, shooting: boolean, reload: boolean): void {
    // 冷却
    if (this._fireCooldown > 0) this._fireCooldown -= dt;

    // 换弹逻辑
    if (this._reloadTimer > 0) {
      this._reloadTimer -= dt;
      if (this._reloadTimer <= 0) {
        this._finishReload();
      }
    }

    // 主动换弹触发
    if (reload && !this.reloading && this.magAmmo < CONFIG.magSize && this.reserveAmmo > 0) {
      this._startReload();
    }

    // 枪口闪光消隐
    if (this._flashTimer > 0) {
      this._flashTimer -= dt;
      if (this._flashTimer <= 0) {
        this._flashMesh.visible = false;
      }
    }

    // 后坐力平滑回正
    if (this._recoilPitch > 0) {
      const recover = Math.min(this._recoilPitch, CONFIG.recoilAmount * 6 * dt);
      this._recoilPitch -= recover;
      this.camera.rotation.x += recover;
    }
  }

  /**
   * 尝试射击，返回命中的 Object3D（或 null）
   * @param scene 场景（用于 Raycaster 与场景内所有 Mesh 求交）
   * @param targets 可击中目标列表
   */
  shoot(
    scene: THREE.Scene,
    targets: THREE.Object3D[],
  ): THREE.Object3D | null {
    if (this.reloading) return null;
    if (this._fireCooldown > 0) return null;
    if (this.magAmmo <= 0) {
      // 弹匣空，自动换弹
      if (!this.reloading && this.reserveAmmo > 0) this._startReload();
      return null;
    }

    // 消耗子弹
    this.magAmmo -= 1;
    this._fireCooldown = CONFIG.fireRate;

    // 后坐力：向上抬
    const currentPitch = this.camera.rotation.x;
    this.camera.rotation.x = Math.max(currentPitch - CONFIG.recoilAmount, -Math.PI / 2);
    this._recoilPitch += CONFIG.recoilAmount;

    // 枪口闪光
    this._flashMesh.visible = true;
    this._flashTimer = CONFIG.flashDuration;

    // Raycaster 从摄像机中心射出
    this._raycaster.setFromCamera(new THREE.Vector2(0, 0), this.camera as THREE.PerspectiveCamera);

    // 只与目标求交
    const hits = this._raycaster.intersectObjects(targets, true);
    if (hits.length > 0 && hits[0] !== undefined) {
      return hits[0].object;
    }
    return null;
  }

  /** 开始换弹 */
  private _startReload(): void {
    this.reloading = true;
    this._reloadTimer = CONFIG.reloadTime;
  }

  /** 换弹完成 */
  private _finishReload(): void {
    const needed = CONFIG.magSize - this.magAmmo;
    const take = Math.min(needed, this.reserveAmmo);
    this.magAmmo += take;
    this.reserveAmmo -= take;
    this.reloading = false;
  }

  /** 重置武器（重新开始游戏） */
  reset(): void {
    this.magAmmo = CONFIG.magSize;
    this.reserveAmmo = CONFIG.reserveAmmo;
    this.reloading = false;
    this._reloadTimer = 0;
    this._fireCooldown = 0;
    this._flashTimer = 0;
    this._flashMesh.visible = false;
    this._recoilPitch = 0;
  }
}
