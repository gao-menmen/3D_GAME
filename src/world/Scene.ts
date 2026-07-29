/**
 * 场景、光照、地形与障碍物
 * 所有几何体均为程序化生成，无第三方版权资源
 */

import * as THREE from 'three';
import type { AABB } from '../types';
import { CONFIG } from '../types';

/** 障碍物描述：位置、宽度、高度、深度 */
interface ObstacleDef {
  x: number;
  y: number;
  z: number;
  w: number;
  h: number;
  d: number;
}

export class GameScene {
  readonly scene: THREE.Scene;
  /** 所有可碰撞障碍物的 AABB 列表 */
  readonly obstacles: AABB[] = [];

  constructor() {
    this.scene = new THREE.Scene();
    this._setupFog();
    this._setupSky();
    this._setupLights();
    this._buildGround();
    this._buildObstacles();
  }

  /** 设置雾效 */
  private _setupFog(): void {
    // 沙漠风格淡黄色大气雾
    this.scene.fog = new THREE.FogExp2(0xc8b88a, 0.012);
  }

  /** 设置天空背景色 */
  private _setupSky(): void {
    this.scene.background = new THREE.Color(0x8fbcd4);
  }

  /** 设置方向光 + 环境光 */
  private _setupLights(): void {
    // 环境光：柔和暖色
    const ambient = new THREE.AmbientLight(0xfff4e0, 0.6);
    this.scene.add(ambient);

    // 主方向光（模拟太阳）
    const sun = new THREE.DirectionalLight(0xfffbe8, 1.2);
    sun.position.set(40, 80, 30);
    sun.castShadow = true;
    sun.shadow.mapSize.set(1024, 1024);
    sun.shadow.camera.near = 0.5;
    sun.shadow.camera.far = 200;
    const d = 80;
    sun.shadow.camera.left = -d;
    sun.shadow.camera.right = d;
    sun.shadow.camera.top = d;
    sun.shadow.camera.bottom = -d;
    this.scene.add(sun);

    // 补光（模拟天空漫反射）
    const fill = new THREE.DirectionalLight(0x88aacc, 0.3);
    fill.position.set(-20, 30, -40);
    this.scene.add(fill);
  }

  /** 构建地面 */
  private _buildGround(): void {
    const geo = new THREE.PlaneGeometry(CONFIG.worldSize, CONFIG.worldSize);
    // 沙漠泥土色地面
    const mat = new THREE.MeshLambertMaterial({ color: 0xc2a46a });
    const ground = new THREE.Mesh(geo, mat);
    ground.rotation.x = -Math.PI / 2;
    ground.receiveShadow = true;
    this.scene.add(ground);

    // 地面网格线，增加真实感
    const gridHelper = new THREE.GridHelper(
      CONFIG.worldSize,
      CONFIG.worldSize / 4,
      0x8a7450,
      0x8a7450,
    );
    gridHelper.position.y = 0.01;
    (gridHelper.material as THREE.LineBasicMaterial).opacity = 0.25;
    (gridHelper.material as THREE.LineBasicMaterial).transparent = true;
    this.scene.add(gridHelper);
  }

  /** 构建障碍物（箱体、墙体、掩体） */
  private _buildObstacles(): void {
    // 障碍物列表：[x, y偏移（底部贴地）, z, 宽, 高, 深]
    // y = h/2 使底部贴地
    const defs: ObstacleDef[] = [
      // 中央掩体群
      { x: 0,    y: 0, z: 0,    w: 4,  h: 2,   d: 1.5 },
      { x: 6,    y: 0, z: 2,    w: 1.5,h: 1.5, d: 4   },
      { x: -6,   y: 0, z: -3,   w: 2,  h: 2.5, d: 1.5 },
      // 沙包/木箱
      { x: 10,   y: 0, z: -10,  w: 1.2,h: 1,   d: 1.2 },
      { x: 11.5, y: 0, z: -10,  w: 1.2,h: 1,   d: 1.2 },
      { x: 10,   y: 0, z: -8.5, w: 1.2,h: 1,   d: 1.2 },
      // 建筑残墙
      { x: -15,  y: 0, z: 10,   w: 8,  h: 3,   d: 0.5 },
      { x: -15,  y: 0, z: 6,    w: 0.5,h: 3,   d: 8   },
      // 油桶群
      { x: 20,   y: 0, z: 5,    w: 1,  h: 1.8, d: 1   },
      { x: 21.5, y: 0, z: 5,    w: 1,  h: 1.8, d: 1   },
      // 碉堡/掩体箱
      { x: -20,  y: 0, z: -20,  w: 5,  h: 2,   d: 5   },
      { x: 25,   y: 0, z: -20,  w: 5,  h: 2,   d: 5   },
      { x: 0,    y: 0, z: 30,   w: 6,  h: 2.5, d: 2   },
      { x: 0,    y: 0, z: -30,  w: 6,  h: 2.5, d: 2   },
      // 外围墙（四面边界）
      { x: 0,    y: 0, z:  75,  w: 150, h: 4, d: 2   },
      { x: 0,    y: 0, z: -75,  w: 150, h: 4, d: 2   },
      { x:  75,  y: 0, z: 0,    w: 2,   h: 4, d: 150 },
      { x: -75,  y: 0, z: 0,    w: 2,   h: 4, d: 150 },
    ];

    // 颜色列表，循环使用
    const colors = [0x8a7860, 0x6b7c5a, 0x7a6040, 0x5c6a4a, 0x9a8060];

    defs.forEach((def, i) => {
      const geo = new THREE.BoxGeometry(def.w, def.h, def.d);
      const mat = new THREE.MeshLambertMaterial({
        color: colors[i % colors.length],
      });
      const mesh = new THREE.Mesh(geo, mat);
      // 底部贴地：y = h/2
      mesh.position.set(def.x, def.h / 2, def.z);
      mesh.castShadow = true;
      mesh.receiveShadow = true;
      this.scene.add(mesh);

      // 注册 AABB
      this.obstacles.push({
        minX: def.x - def.w / 2,
        maxX: def.x + def.w / 2,
        minZ: def.z - def.d / 2,
        maxZ: def.z + def.d / 2,
        minY: 0,
        maxY: def.h,
      });
    });
  }

  /** 重置场景（重新开始游戏时调用） */
  reset(): void {
    // 场景是静态的，无需重建
  }
}
